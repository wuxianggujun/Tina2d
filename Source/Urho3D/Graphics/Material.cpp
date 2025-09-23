// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/Profiler.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Material.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Technique.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../GraphicsAPI/Texture2DArray.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/VectorBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Resource/JSONFile.h"
#include "../Resource/JSONObject.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/ValueAnimation.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* wrapModeNames[];

static const char* textureUnitNames[] =
{
    "diffuse",     // TU_DIFFUSE
    "normal",      // TU_NORMAL
    "specular",    // TU_SPECULAR（2D可保留，不影响）
    "emissive",    // TU_EMISSIVE
    "volume",      // TU_VOLUMEMAP（2D通常不使用）
    "custom1",     // TU_CUSTOM1
    "custom2",     // TU_CUSTOM2
    "zone",        // TU_ZONE
    nullptr
};

const char* cullModeNames[] =
{
    "none",
    "ccw",
    "cw",
    nullptr
};

static const char* fillModeNames[] =
{
    "solid",
    "wireframe",
    "point",
    nullptr
};

TextureUnit ParseTextureUnitName(String name)
{
    name = name.ToLower().Trimmed();

    TextureUnit unit = (TextureUnit)GetStringListIndex(name.CString(), textureUnitNames, MAX_TEXTURE_UNITS);
    if (unit == MAX_TEXTURE_UNITS)
    {
        // 检查简写名（仅限 2D 可用简写）
        if (name == "diff")
            unit = TU_DIFFUSE;
        else if (name == "albedo")
            unit = TU_DIFFUSE;
        else if (name == "norm")
            unit = TU_NORMAL;
        else if (name == "spec")
            unit = TU_SPECULAR;
        // 也允许以数字指定纹理单元索引（如 "0"、"1"）
        else if (name.Length() < 3)
            unit = (TextureUnit)Clamp(ToI32(name), 0, MAX_TEXTURE_UNITS - 1);
    }

    if (unit == MAX_TEXTURE_UNITS)
        URHO3D_LOGERROR("Unknown texture unit name " + name);

    return unit;
}

StringHash ParseTextureTypeName(const String& name)
{
    String lowerCaseName = name.ToLower().Trimmed();

    if (lowerCaseName == "texture")
        return Texture2D::GetTypeStatic();
    else if (lowerCaseName == "cubemap")
        return Texture2D::GetTypeStatic(); // 2D-only：将 cubemap 视作 2D 纹理
    // 2D-only：忽略 3D 纹理类型
    else if (lowerCaseName == "texturearray")
        return Texture2DArray::GetTypeStatic();

    return nullptr;
}

StringHash ParseTextureTypeXml(ResourceCache* cache, const String& filename)
{
    StringHash type = nullptr;
    if (!cache)
        return type;

    SharedPtr<File> texXmlFile = cache->GetFile(filename, false);
    if (texXmlFile.NotNull())
    {
        SharedPtr<XMLFile> texXml(new XMLFile(cache->GetContext()));
        if (texXml->Load(*texXmlFile))
            type = ParseTextureTypeName(texXml->GetRoot().GetName());
    }
    return type;
}

static TechniqueEntry noEntry;

bool CompareTechniqueEntries(const TechniqueEntry& lhs, const TechniqueEntry& rhs)
{
    if (lhs.lodDistance_ != rhs.lodDistance_)
        return lhs.lodDistance_ > rhs.lodDistance_;
    else
        return lhs.qualityLevel_ > rhs.qualityLevel_;
}

TechniqueEntry::TechniqueEntry() noexcept :
    qualityLevel_(QUALITY_LOW),
    lodDistance_(0.0f)
{
}

TechniqueEntry::TechniqueEntry(Technique* tech, MaterialQuality qualityLevel, float lodDistance) noexcept :
    technique_(tech),
    original_(tech),
    qualityLevel_(qualityLevel),
    lodDistance_(lodDistance)
{
}

ShaderParameterAnimationInfo::ShaderParameterAnimationInfo(Material* material, const String& name, ValueAnimation* attributeAnimation,
    WrapMode wrapMode, float speed) :
    ValueAnimationInfo(material, attributeAnimation, wrapMode, speed),
    name_(name)
{
}

ShaderParameterAnimationInfo::ShaderParameterAnimationInfo(const ShaderParameterAnimationInfo& other) = default;

ShaderParameterAnimationInfo::~ShaderParameterAnimationInfo() = default;

void ShaderParameterAnimationInfo::ApplyValue(const Variant& newValue)
{
    static_cast<Material*>(target_.Get())->SetShaderParameter(name_, newValue);
}

Material::Material(Context* context) :
    Resource(context)
{
    ResetToDefaults();
}

Material::~Material() = default;

void Material::RegisterObject(Context* context)
{
    context->RegisterFactory<Material>();
}

bool Material::BeginLoad(Deserializer& source)
{
    // In headless mode, do not actually load the material, just return success
    auto* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return true;

    String extension = GetExtension(source.GetName());

    bool success = false;
    if (extension == ".xml")
    {
        success = BeginLoadXML(source);
        if (!success)
            success = BeginLoadJSON(source);

        if (success)
            return true;
    }
    else // Load JSON file
    {
        success = BeginLoadJSON(source);
        if (!success)
            success = BeginLoadXML(source);

        if (success)
            return true;
    }

    // All loading failed
    ResetToDefaults();
    loadJSONFile_.Reset();
    return false;
}

bool Material::EndLoad()
{
    // In headless mode, do not actually load the material, just return success
    auto* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return true;

    bool success = false;
    if (loadXMLFile_)
    {
        // If async loading, get the techniques / textures which should be ready now
        XMLElement rootElem = loadXMLFile_->GetRoot();
        success = Load(rootElem);
    }

    if (loadJSONFile_)
    {
        JSONValue rootVal = loadJSONFile_->GetRoot();
        success = Load(rootVal);
    }

    loadXMLFile_.Reset();
    loadJSONFile_.Reset();
    return success;
}

bool Material::BeginLoadXML(Deserializer& source)
{
    ResetToDefaults();
    loadXMLFile_ = new XMLFile(context_);
    if (loadXMLFile_->Load(source))
    {
        // 如果是异步加载，仅预请求依赖资源
        if (GetAsyncLoadState() == ASYNC_LOADING)
        {
            auto* cache = GetSubsystem<ResourceCache>();
            XMLElement rootElem = loadXMLFile_->GetRoot();
            // 预加载 Technique 依赖
            XMLElement techniqueElem = rootElem.GetChild("technique");
            while (techniqueElem)
            {
                cache->BackgroundLoadResource<Technique>(techniqueElem.GetAttribute("name"), true, this);
                techniqueElem = techniqueElem.GetNext("technique");
            }
            // 预加载 Texture 依赖
            XMLElement textureElem = rootElem.GetChild("texture");
            while (textureElem)
            {
                String name = textureElem.GetAttribute("name");
                if (GetExtension(name) == ".xml")
                {
                    StringHash type = ParseTextureTypeXml(cache, name);
                    bool handled = false;
                    if (type == Texture2DArray::GetTypeStatic())
                    {
                        cache->BackgroundLoadResource<Texture2DArray>(name, true, this);
                        handled = true;
                    }
                    if (!handled)
                        cache->BackgroundLoadResource<Texture2D>(name, true, this);
                }
                else
                    cache->BackgroundLoadResource<Texture2D>(name, true, this);

                textureElem = textureElem.GetNext("texture");
            }
        }
        return true;
    }
    return false;
}

bool Material::BeginLoadJSON(Deserializer& source)
{
    ResetToDefaults();
    loadJSONFile_ = new JSONFile(context_);
    if (loadJSONFile_->Load(source))
    {
        if (GetAsyncLoadState() == ASYNC_LOADING)
        {
            auto* cache = GetSubsystem<ResourceCache>();
            const JSONValue& rootVal = loadJSONFile_->GetRoot();

            // 预请求 techniques
            JSONArray techniqueArray = rootVal.Get("techniques").GetArray();
            for (const JSONValue& techVal : techniqueArray)
                cache->BackgroundLoadResource<Technique>(techVal.Get("name").GetString(), true, this);

            // 预请求 textures
            JSONObject textureObject = rootVal.Get("textures").GetObject();
            for (JSONObject::ConstIterator it = textureObject.Begin(); it != textureObject.End(); ++it)
            {
                String name = it->second.GetString();
                if (GetExtension(name) == ".xml")
                {
                    StringHash type = ParseTextureTypeXml(cache, name);
                    bool handled = false;
                    if (type == Texture2DArray::GetTypeStatic())
                    {
                        cache->BackgroundLoadResource<Texture2DArray>(name, true, this);
                        handled = true;
                    }
                    if (!handled)
                        cache->BackgroundLoadResource<Texture2D>(name, true, this);
                }
                else
                    cache->BackgroundLoadResource<Texture2D>(name, true, this);
            }
        }
        return true;
    }
    return false;
}

bool Material::Save(Serializer& dest) const
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement root = xml->CreateRoot("material");
    if (!Save(root))
        return false;
    return xml->Save(dest);
}

bool Material::Load(const XMLElement& source)
{
    ResetToDefaults();

    if (source.IsNull())
    {
        URHO3D_LOGERROR("Can not load material from null XML element");
        return false;
    }

    auto* cache = GetSubsystem<ResourceCache>();

    // 着色器宏
    XMLElement shaderElem = source.GetChild("shader");
    if (shaderElem)
    {
        vertexShaderDefines_ = shaderElem.GetAttribute("vsdefines");
        pixelShaderDefines_ = shaderElem.GetAttribute("psdefines");
    }

    // 技术列表
    XMLElement techniqueElem = source.GetChild("technique");
    techniques_.Clear();
    while (techniqueElem)
    {
        Technique* tech = cache->GetResource<Technique>(techniqueElem.GetAttribute("name"));
        if (tech)
        {
            TechniqueEntry newTechnique;
            newTechnique.technique_ = newTechnique.original_ = tech;
            if (techniqueElem.HasAttribute("quality"))
                newTechnique.qualityLevel_ = (MaterialQuality)techniqueElem.GetI32("quality");
            if (techniqueElem.HasAttribute("loddistance"))
                newTechnique.lodDistance_ = techniqueElem.GetFloat("loddistance");
            techniques_.Push(newTechnique);
        }
        techniqueElem = techniqueElem.GetNext("technique");
    }

    SortTechniques();
    ApplyShaderDefines();

    // 纹理列表
    XMLElement textureElem = source.GetChild("texture");
    while (textureElem)
    {
        TextureUnit unit = TU_DIFFUSE;
        if (textureElem.HasAttribute("unit"))
            unit = ParseTextureUnitName(textureElem.GetAttribute("unit"));
        if (unit < MAX_TEXTURE_UNITS)
        {
            String name = textureElem.GetAttribute("name");
            if (GetExtension(name) == ".xml")
            {
                StringHash type = ParseTextureTypeXml(cache, name);
                bool handled = false;
                if (type == Texture2DArray::GetTypeStatic())
                {
                    SetTexture(unit, cache->GetResource<Texture2DArray>(name));
                    handled = true;
                }
                if (!handled)
                    SetTexture(unit, cache->GetResource<Texture2D>(name));
            }
            else
                SetTexture(unit, cache->GetResource<Texture2D>(name));
        }
        textureElem = textureElem.GetNext("texture");
    }

    // 参数（批量）
    batchedParameterUpdate_ = true;
    XMLElement parameterElem = source.GetChild("parameter");
    while (parameterElem)
    {
        String name = parameterElem.GetAttribute("name");
        if (!parameterElem.HasAttribute("type"))
            SetShaderParameter(name, ParseShaderParameterValue(parameterElem.GetAttribute("value")));
        else
            SetShaderParameter(name, Variant(parameterElem.GetAttribute("type"), parameterElem.GetAttribute("value")));
        parameterElem = parameterElem.GetNext("parameter");
    }
    batchedParameterUpdate_ = false;

    // 参数动画
    XMLElement parameterAnimationElem = source.GetChild("parameteranimation");
    while (parameterAnimationElem)
    {
        String name = parameterAnimationElem.GetAttribute("name");
        SharedPtr<ValueAnimation> animation(new ValueAnimation(context_));
        if (!animation->LoadXML(parameterAnimationElem))
        {
            URHO3D_LOGERROR("Could not load parameter animation");
            return false;
        }

        String wrapModeString = parameterAnimationElem.GetAttribute("wrapmode");
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = parameterAnimationElem.GetFloat("speed");
        SetShaderParameterAnimation(name, animation, wrapMode, speed);

        parameterAnimationElem = parameterAnimationElem.GetNext("parameteranimation");
    }

    // 其他属性
    XMLElement cullElem = source.GetChild("cull");
    if (cullElem)
        SetCullMode((CullMode)GetStringListIndex(cullElem.GetAttribute("value").CString(), cullModeNames, CULL_CCW));

    XMLElement shadowCullElem = source.GetChild("shadowcull");
    if (shadowCullElem)
        SetShadowCullMode((CullMode)GetStringListIndex(shadowCullElem.GetAttribute("value").CString(), cullModeNames, CULL_CCW));

    XMLElement fillElem = source.GetChild("fill");
    if (fillElem)
        SetFillMode((FillMode)GetStringListIndex(fillElem.GetAttribute("value").CString(), fillModeNames, FILL_SOLID));

    XMLElement depthBiasElem = source.GetChild("depthbias");
    if (depthBiasElem)
        SetDepthBias(BiasParameters(depthBiasElem.GetFloat("constant"), depthBiasElem.GetFloat("slopescaled")));

    XMLElement alphaToCoverageElem = source.GetChild("alphatocoverage");
    if (alphaToCoverageElem)
        SetAlphaToCoverage(alphaToCoverageElem.GetBool("enable"));

    XMLElement lineAntiAliasElem = source.GetChild("lineantialias");
    if (lineAntiAliasElem)
        SetLineAntiAlias(lineAntiAliasElem.GetBool("enable"));

    XMLElement renderOrderElem = source.GetChild("renderorder");
    if (renderOrderElem)
        SetRenderOrder((i8)renderOrderElem.GetI32("value"));

    XMLElement occlusionElem = source.GetChild("occlusion");
    if (occlusionElem)
        SetOcclusion(occlusionElem.GetBool("enable"));

    RefreshShaderParameterHash();
    RefreshMemoryUse();
    return true;
}

bool Material::Load(const JSONValue& source)
{
    ResetToDefaults();

    if (source.IsNull())
    {
        URHO3D_LOGERROR("Can not load material from null JSON element");
        return false;
    }

    auto* cache = GetSubsystem<ResourceCache>();

    const JSONValue& shaderVal = source.Get("shader");
    if (!shaderVal.IsNull())
    {
        vertexShaderDefines_ = shaderVal.Get("vsdefines").GetString();
        pixelShaderDefines_ = shaderVal.Get("psdefines").GetString();
    }

    // Load techniques
    JSONArray techniquesArray = source.Get("techniques").GetArray();
    techniques_.Clear();
    techniques_.Reserve(techniquesArray.Size());

    for (const JSONValue& techVal : techniquesArray)
    {
        Technique* tech = cache->GetResource<Technique>(techVal.Get("name").GetString());
        if (tech)
        {
            TechniqueEntry newTechnique;
            newTechnique.technique_ = newTechnique.original_ = tech;
            JSONValue qualityVal = techVal.Get("quality");
            if (!qualityVal.IsNull())
                newTechnique.qualityLevel_ = (MaterialQuality)qualityVal.GetI32();
            JSONValue lodDistanceVal = techVal.Get("loddistance");
            if (!lodDistanceVal.IsNull())
                newTechnique.lodDistance_ = lodDistanceVal.GetFloat();
            techniques_.Push(newTechnique);
        }
    }

    SortTechniques();
    ApplyShaderDefines();

    // Load textures
    JSONObject textureObject = source.Get("textures").GetObject();
    for (JSONObject::ConstIterator it = textureObject.Begin(); it != textureObject.End(); ++it)
    {
        String textureUnit = it->first;
        String textureName = it->second.GetString();

        TextureUnit unit = TU_DIFFUSE;
        unit = ParseTextureUnitName(textureUnit);

        if (unit < MAX_TEXTURE_UNITS)
        {
            // Detect cube maps and arrays by file extension: they are defined by an XML file
            if (GetExtension(textureName) == ".xml")
            {
// 桌面/GLES3 平台：识别 3D/数组/Cube map 的 XML 包装类型
                StringHash type = ParseTextureTypeXml(cache, textureName);
                if (!type && unit == TU_VOLUMEMAP)
                {
// 2D-only：忽略 3D 体积纹理
                }

                bool handled = false;

                if (!handled && type == Texture2DArray::GetTypeStatic())
                {
                    SetTexture(unit, cache->GetResource<Texture2DArray>(textureName));
                    handled = true;
                }
                if (!handled)
                    SetTexture(unit, cache->GetResource<Texture2D>(textureName));
            }
            else
                SetTexture(unit, cache->GetResource<Texture2D>(textureName));
        }
    }

    // Get shader parameters
    batchedParameterUpdate_ = true;
    JSONObject parameterObject = source.Get("shaderParameters").GetObject();

    for (JSONObject::ConstIterator it = parameterObject.Begin(); it != parameterObject.End(); ++it)
    {
        String name = it->first;
        if (it->second.IsString())
            SetShaderParameter(name, ParseShaderParameterValue(it->second.GetString()));
        else if (it->second.IsObject())
        {
            JSONObject valueObj = it->second.GetObject();
            SetShaderParameter(name, Variant(valueObj["type"].GetString(), valueObj["value"].GetString()));
        }
    }
    batchedParameterUpdate_ = false;

    // Load shader parameter animations
    JSONObject paramAnimationsObject = source.Get("shaderParameterAnimations").GetObject();
    for (JSONObject::ConstIterator it = paramAnimationsObject.Begin(); it != paramAnimationsObject.End(); ++it)
    {
        String name = it->first;
        JSONValue paramAnimVal = it->second;

        SharedPtr<ValueAnimation> animation(new ValueAnimation(context_));
        if (!animation->LoadJSON(paramAnimVal))
        {
            URHO3D_LOGERROR("Could not load parameter animation");
            return false;
        }

        String wrapModeString = paramAnimVal.Get("wrapmode").GetString();
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = paramAnimVal.Get("speed").GetFloat();
        SetShaderParameterAnimation(name, animation, wrapMode, speed);
    }

    JSONValue cullVal = source.Get("cull");
    if (!cullVal.IsNull())
        SetCullMode((CullMode)GetStringListIndex(cullVal.GetString().CString(), cullModeNames, CULL_CCW));

    JSONValue shadowCullVal = source.Get("shadowcull");
    if (!shadowCullVal.IsNull())
        SetShadowCullMode((CullMode)GetStringListIndex(shadowCullVal.GetString().CString(), cullModeNames, CULL_CCW));

    JSONValue fillVal = source.Get("fill");
    if (!fillVal.IsNull())
        SetFillMode((FillMode)GetStringListIndex(fillVal.GetString().CString(), fillModeNames, FILL_SOLID));

    JSONValue depthBiasVal = source.Get("depthbias");
    if (!depthBiasVal.IsNull())
        SetDepthBias(BiasParameters(depthBiasVal.Get("constant").GetFloat(), depthBiasVal.Get("slopescaled").GetFloat()));

    JSONValue alphaToCoverageVal = source.Get("alphatocoverage");
    if (!alphaToCoverageVal.IsNull())
        SetAlphaToCoverage(alphaToCoverageVal.GetBool());

    JSONValue lineAntiAliasVal = source.Get("lineantialias");
    if (!lineAntiAliasVal.IsNull())
        SetLineAntiAlias(lineAntiAliasVal.GetBool());

    JSONValue renderOrderVal = source.Get("renderorder");
    if (!renderOrderVal.IsNull())
        SetRenderOrder((i8)renderOrderVal.GetI32());

    JSONValue occlusionVal = source.Get("occlusion");
    if (!occlusionVal.IsNull())
        SetOcclusion(occlusionVal.GetBool());

    RefreshShaderParameterHash();
    RefreshMemoryUse();
    return true;
}

bool Material::Save(XMLElement& dest) const
{
    if (dest.IsNull())
    {
        URHO3D_LOGERROR("Can not save material to null XML element");
        return false;
    }

    // Write techniques
    for (const TechniqueEntry& entry : techniques_)
    {
        if (!entry.technique_)
            continue;

        XMLElement techniqueElem = dest.CreateChild("technique");
        techniqueElem.SetString("name", entry.technique_->GetName());
        techniqueElem.SetI32("quality", entry.qualityLevel_);
        techniqueElem.SetFloat("loddistance", entry.lodDistance_);
    }

    // Write texture units
    for (unsigned j = 0; j < MAX_TEXTURE_UNITS; ++j)
    {
        Texture* texture = GetTexture((TextureUnit)j);
        if (texture)
        {
            XMLElement textureElem = dest.CreateChild("texture");
            textureElem.SetString("unit", textureUnitNames[j]);
            textureElem.SetString("name", texture->GetName());
        }
    }

    // Write shader compile defines
    if (!vertexShaderDefines_.Empty() || !pixelShaderDefines_.Empty())
    {
        XMLElement shaderElem = dest.CreateChild("shader");
        if (!vertexShaderDefines_.Empty())
            shaderElem.SetString("vsdefines", vertexShaderDefines_);
        if (!pixelShaderDefines_.Empty())
            shaderElem.SetString("psdefines", pixelShaderDefines_);
    }

    // Write shader parameters
    for (auto j = shaderParameters_.begin();
    j != shaderParameters_.end(); ++j)
    {
        XMLElement parameterElem = dest.CreateChild("parameter");
        parameterElem.SetString("name", j->second.name_);
        if (j->second.value_.GetType() != VAR_BUFFER && j->second.value_.GetType() != VAR_INT && j->second.value_.GetType() != VAR_BOOL)
            parameterElem.SetVectorVariant("value", j->second.value_);
        else
        {
            parameterElem.SetAttribute("type", j->second.value_.GetTypeName());
            parameterElem.SetAttribute("value", j->second.value_.ToString());
        }
    }

    // Write shader parameter animations
    for (auto j = shaderParameterAnimationInfos_.begin();
    j != shaderParameterAnimationInfos_.end(); ++j)
    {
        ShaderParameterAnimationInfo* info = j->second;
        XMLElement parameterAnimationElem = dest.CreateChild("parameteranimation");
        parameterAnimationElem.SetString("name", info->GetName());
        if (!info->GetAnimation()->SaveXML(parameterAnimationElem))
            return false;

        parameterAnimationElem.SetAttribute("wrapmode", wrapModeNames[info->GetWrapMode()]);
        parameterAnimationElem.SetFloat("speed", info->GetSpeed());
    }

    // Write culling modes
    XMLElement cullElem = dest.CreateChild("cull");
    cullElem.SetString("value", cullModeNames[cullMode_]);

    XMLElement shadowCullElem = dest.CreateChild("shadowcull");
    shadowCullElem.SetString("value", cullModeNames[shadowCullMode_]);

    // Write fill mode
    XMLElement fillElem = dest.CreateChild("fill");
    fillElem.SetString("value", fillModeNames[fillMode_]);

    // Write depth bias
    XMLElement depthBiasElem = dest.CreateChild("depthbias");
    depthBiasElem.SetFloat("constant", depthBias_.constantBias_);
    depthBiasElem.SetFloat("slopescaled", depthBias_.slopeScaledBias_);

    // Write alpha-to-coverage
    XMLElement alphaToCoverageElem = dest.CreateChild("alphatocoverage");
    alphaToCoverageElem.SetBool("enable", alphaToCoverage_);

    // Write line anti-alias
    XMLElement lineAntiAliasElem = dest.CreateChild("lineantialias");
    lineAntiAliasElem.SetBool("enable", lineAntiAlias_);

    // Write render order
    XMLElement renderOrderElem = dest.CreateChild("renderorder");
    renderOrderElem.SetI32("value", renderOrder_);

    // Write occlusion
    XMLElement occlusionElem = dest.CreateChild("occlusion");
    occlusionElem.SetBool("enable", occlusion_);

    return true;
}

bool Material::Save(JSONValue& dest) const
{
    // Write techniques
    JSONArray techniquesArray;
    techniquesArray.Reserve(techniques_.Size());
    for (const TechniqueEntry& entry : techniques_)
    {
        if (!entry.technique_)
            continue;

        JSONValue techniqueVal;
        techniqueVal.Set("name", entry.technique_->GetName());
        techniqueVal.Set("quality", (int) entry.qualityLevel_);
        techniqueVal.Set("loddistance", entry.lodDistance_);
        techniquesArray.Push(techniqueVal);
    }
    dest.Set("techniques", techniquesArray);

    // Write texture units
    JSONValue texturesValue;
    for (unsigned j = 0; j < MAX_TEXTURE_UNITS; ++j)
    {
        Texture* texture = GetTexture((TextureUnit)j);
        if (texture)
            texturesValue.Set(textureUnitNames[j], texture->GetName());
    }
    dest.Set("textures", texturesValue);

    // Write shader compile defines
    if (!vertexShaderDefines_.Empty() || !pixelShaderDefines_.Empty())
    {
        JSONValue shaderVal;
        if (!vertexShaderDefines_.Empty())
            shaderVal.Set("vsdefines", vertexShaderDefines_);
        if (!pixelShaderDefines_.Empty())
            shaderVal.Set("psdefines", pixelShaderDefines_);
        dest.Set("shader", shaderVal);
    }

    // Write shader parameters
    JSONValue shaderParamsVal;
    for (auto j = shaderParameters_.begin();
    j != shaderParameters_.end(); ++j)
    {
        if (j->second.value_.GetType() != VAR_BUFFER && j->second.value_.GetType() != VAR_INT && j->second.value_.GetType() != VAR_BOOL)
            shaderParamsVal.Set(j->second.name_, j->second.value_.ToString());
        else
        {
            JSONValue valueObj;
            valueObj.Set("type", j->second.value_.GetTypeName());
            valueObj.Set("value", j->second.value_.ToString());
            shaderParamsVal.Set(j->second.name_, valueObj);
        }
    }
    dest.Set("shaderParameters", shaderParamsVal);

    // Write shader parameter animations
    JSONValue shaderParamAnimationsVal;
    for (auto j = shaderParameterAnimationInfos_.begin();
    j != shaderParameterAnimationInfos_.end(); ++j)
    {
        ShaderParameterAnimationInfo* info = j->second;
        JSONValue paramAnimationVal;
        if (!info->GetAnimation()->SaveJSON(paramAnimationVal))
            return false;

        paramAnimationVal.Set("wrapmode", wrapModeNames[info->GetWrapMode()]);
        paramAnimationVal.Set("speed", info->GetSpeed());
        shaderParamAnimationsVal.Set(info->GetName(), paramAnimationVal);
    }
    dest.Set("shaderParameterAnimations", shaderParamAnimationsVal);

    // Write culling modes
    dest.Set("cull", cullModeNames[cullMode_]);
    dest.Set("shadowcull", cullModeNames[shadowCullMode_]);

    // Write fill mode
    dest.Set("fill", fillModeNames[fillMode_]);

    // Write depth bias
    JSONValue depthBiasValue;
    depthBiasValue.Set("constant", depthBias_.constantBias_);
    depthBiasValue.Set("slopescaled", depthBias_.slopeScaledBias_);
    dest.Set("depthbias", depthBiasValue);

    // Write alpha-to-coverage
    dest.Set("alphatocoverage", alphaToCoverage_);

    // Write line anti-alias
    dest.Set("lineantialias", lineAntiAlias_);

    // Write render order
    dest.Set("renderorder", renderOrder_);

    // Write occlusion
    dest.Set("occlusion", occlusion_);

    return true;
}

void Material::SetNumTechniques(i32 num)
{
    assert(num >= 0);

    if (!num)
        return;

    techniques_.Resize(num);
    RefreshMemoryUse();
}

void Material::SetTechnique(i32 index, Technique* tech, MaterialQuality qualityLevel, float lodDistance)
{
    assert(index >= 0);

    if (index >= techniques_.Size())
        return;

    techniques_[index] = TechniqueEntry(tech, qualityLevel, lodDistance);
    ApplyShaderDefines(index);
}

void Material::SetVertexShaderDefines(const String& defines)
{
    if (defines != vertexShaderDefines_)
    {
        vertexShaderDefines_ = defines;
        ApplyShaderDefines();
    }
}

void Material::SetPixelShaderDefines(const String& defines)
{
    if (defines != pixelShaderDefines_)
    {
        pixelShaderDefines_ = defines;
        ApplyShaderDefines();
    }
}

void Material::SetShaderParameter(const String& name, const Variant& value)
{
    MaterialShaderParameter newParam;
    newParam.name_ = name;
    newParam.value_ = value;

    StringHash nameHash(name);
    shaderParameters_[nameHash] = newParam;

    if (nameHash == PSP_MATSPECCOLOR)
    {
        VariantType type = value.GetType();
        if (type == VAR_VECTOR3)
        {
            const Vector3& vec = value.GetVector3();
            specular_ = vec.x_ > 0.0f || vec.y_ > 0.0f || vec.z_ > 0.0f;
        }
        else if (type == VAR_VECTOR4)
        {
            const Vector4& vec = value.GetVector4();
            specular_ = vec.x_ > 0.0f || vec.y_ > 0.0f || vec.z_ > 0.0f;
        }
    }

    if (!batchedParameterUpdate_)
    {
        RefreshShaderParameterHash();
        RefreshMemoryUse();
    }
}

void Material::SetShaderParameterAnimation(const String& name, ValueAnimation* animation, WrapMode wrapMode, float speed)
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);

    if (animation)
    {
        if (info && info->GetAnimation() == animation)
        {
            info->SetWrapMode(wrapMode);
            info->SetSpeed(speed);
            return;
        }

        if (shaderParameters_.find(name) == shaderParameters_.end())
        {
            URHO3D_LOGERROR(GetName() + " has no shader parameter: " + name);
            return;
        }

        StringHash nameHash(name);
        shaderParameterAnimationInfos_[nameHash] = new ShaderParameterAnimationInfo(this, name, animation, wrapMode, speed);
        UpdateEventSubscription();
    }
    else
    {
        if (info)
        {
            StringHash nameHash(name);
            shaderParameterAnimationInfos_.erase(nameHash);
            UpdateEventSubscription();
        }
    }
}

void Material::SetShaderParameterAnimationWrapMode(const String& name, WrapMode wrapMode)
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);
    if (info)
        info->SetWrapMode(wrapMode);
}

void Material::SetShaderParameterAnimationSpeed(const String& name, float speed)
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);
    if (info)
        info->SetSpeed(speed);
}

void Material::SetTexture(TextureUnit unit, Texture* texture)
{
    if (unit < MAX_TEXTURE_UNITS)
    {
        if (texture)
            textures_[unit] = texture;
        else
            textures_.erase(unit);
    }
}

void Material::SetUVTransform(const Vector2& offset, float rotation, const Vector2& repeat)
{
    Matrix3x4 transform(Matrix3x4::IDENTITY);
    transform.m00_ = repeat.x_;
    transform.m11_ = repeat.y_;

    Matrix3x4 rotationMatrix(Matrix3x4::IDENTITY);
    rotationMatrix.m00_ = Cos(rotation);
    rotationMatrix.m01_ = Sin(rotation);
    rotationMatrix.m10_ = -rotationMatrix.m01_;
    rotationMatrix.m11_ = rotationMatrix.m00_;
    rotationMatrix.m03_ = 0.5f - 0.5f * (rotationMatrix.m00_ + rotationMatrix.m01_);
    rotationMatrix.m13_ = 0.5f - 0.5f * (rotationMatrix.m10_ + rotationMatrix.m11_);

    transform = transform * rotationMatrix;

    Matrix3x4 offsetMatrix = Matrix3x4::IDENTITY;
    offsetMatrix.m03_ = offset.x_;
    offsetMatrix.m13_ = offset.y_;

    transform = offsetMatrix * transform;

    SetShaderParameter("UOffset", Vector4(transform.m00_, transform.m01_, transform.m02_, transform.m03_));
    SetShaderParameter("VOffset", Vector4(transform.m10_, transform.m11_, transform.m12_, transform.m13_));
}

void Material::SetUVTransform(const Vector2& offset, float rotation, float repeat)
{
    SetUVTransform(offset, rotation, Vector2(repeat, repeat));
}

void Material::SetCullMode(CullMode mode)
{
    cullMode_ = mode;
}

void Material::SetShadowCullMode(CullMode mode)
{
    shadowCullMode_ = mode;
}

void Material::SetFillMode(FillMode mode)
{
    fillMode_ = mode;
}

void Material::SetDepthBias(const BiasParameters& parameters)
{
    depthBias_ = parameters;
    depthBias_.Validate();
}

void Material::SetAlphaToCoverage(bool enable)
{
    alphaToCoverage_ = enable;
}

void Material::SetLineAntiAlias(bool enable)
{
    lineAntiAlias_ = enable;
}

void Material::SetRenderOrder(i8 order)
{
    renderOrder_ = order;
}

void Material::SetOcclusion(bool enable)
{
    occlusion_ = enable;
}

void Material::SetScene(Scene* scene)
{
    UnsubscribeFromEvent(E_UPDATE);
    UnsubscribeFromEvent(E_ATTRIBUTEANIMATIONUPDATE);
    subscribed_ = false;
    scene_ = scene;
    UpdateEventSubscription();
}

void Material::RemoveShaderParameter(const String& name)
{
    StringHash nameHash(name);
    shaderParameters_.erase(nameHash);

    if (nameHash == PSP_MATSPECCOLOR)
        specular_ = false;

    RefreshShaderParameterHash();
    RefreshMemoryUse();
}

void Material::ReleaseShaders()
{
    for (i32 i = 0; i < techniques_.Size(); ++i)
    {
        Technique* tech = techniques_[i].technique_;
        if (tech)
            tech->ReleaseShaders();
    }
}

SharedPtr<Material> Material::Clone(const String& cloneName) const
{
    SharedPtr<Material> ret(new Material(context_));

    ret->SetName(cloneName);
    ret->techniques_ = techniques_;
    ret->vertexShaderDefines_ = vertexShaderDefines_;
    ret->pixelShaderDefines_ = pixelShaderDefines_;
    ret->shaderParameters_ = shaderParameters_;
    ret->shaderParameterHash_ = shaderParameterHash_;
    ret->textures_ = textures_;
    ret->depthBias_ = depthBias_;
    ret->alphaToCoverage_ = alphaToCoverage_;
    ret->lineAntiAlias_ = lineAntiAlias_;
    ret->occlusion_ = occlusion_;
    ret->specular_ = specular_;
    ret->cullMode_ = cullMode_;
    ret->shadowCullMode_ = shadowCullMode_;
    ret->fillMode_ = fillMode_;
    ret->renderOrder_ = renderOrder_;
    ret->RefreshMemoryUse();

    return ret;
}

void Material::SortTechniques()
{
    Sort(techniques_.Begin(), techniques_.End(), CompareTechniqueEntries);
}

void Material::MarkForAuxView(i32 frameNumber)
{
    assert(frameNumber > 0);
    auxViewFrameNumber_ = frameNumber;
}

const TechniqueEntry& Material::GetTechniqueEntry(i32 index) const
{
    assert(index >= 0);
    return index < techniques_.Size() ? techniques_[index] : noEntry;
}

Technique* Material::GetTechnique(i32 index) const
{
    assert(index >= 0);
    return index < techniques_.Size() ? techniques_[index].technique_ : nullptr;
}

Pass* Material::GetPass(i32 index, const String& passName) const
{
    assert(index >= 0);
    Technique* tech = index < techniques_.Size() ? techniques_[index].technique_ : nullptr;
    return tech ? tech->GetPass(passName) : nullptr;
}

Texture* Material::GetTexture(TextureUnit unit) const
{
    auto i = textures_.find(unit);
    return i != textures_.end() ? i->second.Get() : nullptr;
}

const Variant& Material::GetShaderParameter(const String& name) const
{
    auto i = shaderParameters_.find(name);
    return i != shaderParameters_.end() ? i->second.value_ : Variant::EMPTY;
}

ValueAnimation* Material::GetShaderParameterAnimation(const String& name) const
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);
    return info == nullptr ? nullptr : info->GetAnimation();
}

WrapMode Material::GetShaderParameterAnimationWrapMode(const String& name) const
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);
    return info == nullptr ? WM_LOOP : info->GetWrapMode();
}

float Material::GetShaderParameterAnimationSpeed(const String& name) const
{
    ShaderParameterAnimationInfo* info = GetShaderParameterAnimationInfo(name);
    return info == nullptr ? 0 : info->GetSpeed();
}

Scene* Material::GetScene() const
{
    return scene_;
}

String Material::GetTextureUnitName(TextureUnit unit)
{
    return textureUnitNames[unit];
}

Variant Material::ParseShaderParameterValue(const String& value)
{
    String valueTrimmed = value.Trimmed();
    if (valueTrimmed.Length() && IsAlpha((unsigned)valueTrimmed[0]))
        return Variant(ToBool(valueTrimmed));
    else
        return ToVectorVariant(valueTrimmed);
}

void Material::ResetToDefaults()
{
    // Needs to be a no-op when async loading, as this does a GetResource() which is not allowed from worker threads
    if (!Thread::IsMainThread())
        return;

    vertexShaderDefines_.Clear();
    pixelShaderDefines_.Clear();

    SetNumTechniques(1);
    auto* renderer = GetSubsystem<Renderer>();
    SetTechnique(0, renderer ? renderer->GetDefaultTechnique() :
        GetSubsystem<ResourceCache>()->GetResource<Technique>("Techniques/NoTextureUnlit.xml"));

    textures_.clear();

    batchedParameterUpdate_ = true;
    shaderParameters_.clear();
    shaderParameterAnimationInfos_.clear();
    SetShaderParameter("UOffset", Vector4(1.0f, 0.0f, 0.0f, 0.0f));
    SetShaderParameter("VOffset", Vector4(0.0f, 1.0f, 0.0f, 0.0f));
    SetShaderParameter("MatDiffColor", Vector4::ONE);
    SetShaderParameter("MatEmissiveColor", Vector3::ZERO);
    SetShaderParameter("MatSpecColor", Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    SetShaderParameter("Roughness", 0.5f);
    SetShaderParameter("Metallic", 0.0f);
    batchedParameterUpdate_ = false;

    cullMode_ = CULL_CCW;
    shadowCullMode_ = CULL_CCW;
    fillMode_ = FILL_SOLID;
    depthBias_ = BiasParameters(0.0f, 0.0f);
    renderOrder_ = DEFAULT_RENDER_ORDER;
    occlusion_ = true;

    UpdateEventSubscription();
    RefreshShaderParameterHash();
    RefreshMemoryUse();
}

void Material::RefreshShaderParameterHash()
{
    VectorBuffer temp;
    for (auto i = shaderParameters_.begin();
    i != shaderParameters_.end(); ++i)
    {
        temp.WriteStringHash(i->first);
        temp.WriteVariant(i->second.value_);
    }

    shaderParameterHash_ = 0;
    const byte* data = temp.GetData();
    unsigned dataSize = temp.GetSize();
    for (unsigned i = 0; i < dataSize; ++i)
        shaderParameterHash_ = SDBMHash(shaderParameterHash_, data[i]);
}

void Material::RefreshMemoryUse()
{
    unsigned memoryUse = sizeof(Material);

    memoryUse += techniques_.Size() * sizeof(TechniqueEntry);
    memoryUse += MAX_TEXTURE_UNITS * sizeof(SharedPtr<Texture>);
    memoryUse += (unsigned)shaderParameters_.size() * sizeof(MaterialShaderParameter);

    SetMemoryUse(memoryUse);
}

ShaderParameterAnimationInfo* Material::GetShaderParameterAnimationInfo(const String& name) const
{
    StringHash nameHash(name);
    auto i = shaderParameterAnimationInfos_.find(nameHash);
    if (i == shaderParameterAnimationInfos_.end())
        return nullptr;
    return i->second;
}

void Material::UpdateEventSubscription()
{
    if (!shaderParameterAnimationInfos_.empty() && !subscribed_)
    {
        if (scene_)
            SubscribeToEvent(scene_, E_ATTRIBUTEANIMATIONUPDATE, URHO3D_HANDLER(Material, HandleAttributeAnimationUpdate));
        else
            SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(Material, HandleAttributeAnimationUpdate));
        subscribed_ = true;
    }
    else if (subscribed_ && shaderParameterAnimationInfos_.empty())
    {
        UnsubscribeFromEvent(E_UPDATE);
        UnsubscribeFromEvent(E_ATTRIBUTEANIMATIONUPDATE);
        subscribed_ = false;
    }
}

void Material::HandleAttributeAnimationUpdate(StringHash eventType, VariantMap& eventData)
{
    // Timestep parameter is same no matter what event is being listened to
    float timeStep = eventData[Update::P_TIMESTEP].GetFloat();

    // Keep weak pointer to self to check for destruction caused by event handling
    WeakPtr<Object> self(this);

    Vector<String> finishedNames;
    for (auto i = shaderParameterAnimationInfos_.begin();
    i != shaderParameterAnimationInfos_.end(); ++i)
    {
        bool finished = i->second->Update(timeStep);
        // If self deleted as a result of an event sent during animation playback, nothing more to do
        if (self.Expired())
            return;

        if (finished)
            finishedNames.Push(i->second->GetName());
    }

    // Remove finished animations
    for (const String& finishedName : finishedNames)
        SetShaderParameterAnimation(finishedName, nullptr);
}

void Material::ApplyShaderDefines(i32 index/* = NINDEX*/)
{
    assert(index >= 0 || index == NINDEX);

    if (index == NINDEX)
    {
        for (i32 i = 0; i < techniques_.Size(); ++i)
            ApplyShaderDefines(i);
        return;
    }

    if (index >= techniques_.Size() || !techniques_[index].original_)
        return;

    if (vertexShaderDefines_.Empty() && pixelShaderDefines_.Empty())
        techniques_[index].technique_ = techniques_[index].original_;
    else
        techniques_[index].technique_ = techniques_[index].original_->CloneWithDefines(vertexShaderDefines_, pixelShaderDefines_);
}

}


