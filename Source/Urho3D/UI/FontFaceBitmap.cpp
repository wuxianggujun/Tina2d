// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Graphics/Graphics.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../UI/Font.h"
#include "../UI/FontFaceBitmap.h"
#include "../UI/UI.h"

#include "../DebugNew.h"

namespace Urho3D
{

FontFaceBitmap::FontFaceBitmap(Font* font) :
    FontFace(font)
{
}

FontFaceBitmap::~FontFaceBitmap() = default;

// FIXME: The Load() and Save() should be refactored accordingly after the recent FontGlyph struct changes

bool FontFaceBitmap::Load(const unsigned char* fontData, unsigned fontDataSize, float pointSize)
{
    Context* context = font_->GetContext();

    SharedPtr<XMLFile> xmlReader(new XMLFile(context));
    MemoryBuffer memoryBuffer(fontData, fontDataSize);
    if (!xmlReader->Load(memoryBuffer))
    {
        URHO3D_LOGERROR("Could not load XML file");
        return false;
    }

    XMLElement root = xmlReader->GetRoot("font");
    if (root.IsNull())
    {
        URHO3D_LOGERROR("Could not find Font element");
        return false;
    }

    XMLElement pagesElem = root.GetChild("pages");
    if (pagesElem.IsNull())
    {
        URHO3D_LOGERROR("Could not find Pages element");
        return false;
    }

    XMLElement infoElem = root.GetChild("info");
    if (!infoElem.IsNull())
        pointSize_ = (float)infoElem.GetI32("size");

    XMLElement commonElem = root.GetChild("common");
    rowHeight_ = (float)commonElem.GetI32("lineHeight");
    unsigned pages = commonElem.GetU32("pages");
    textures_.Reserve(pages);

    auto* resourceCache = font_->GetSubsystem<ResourceCache>();
    String fontPath = GetPath(font_->GetName());
    unsigned totalTextureSize = 0;

    XMLElement pageElem = pagesElem.GetChild("page");
    for (unsigned i = 0; i < pages; ++i)
    {
        if (pageElem.IsNull())
        {
            URHO3D_LOGERROR("Could not find Page element for page: " + String(i));
            return false;
        }

        // Assume the font image is in the same directory as the font description file
        String textureFile = fontPath + pageElem.GetAttribute("file");

        // Load texture manually to allow controlling the alpha channel mode
        SharedPtr<File> fontFile = resourceCache->GetFile(textureFile);
        SharedPtr<Image> fontImage(new Image(context));
        if (!fontFile || !fontImage->Load(*fontFile))
        {
            URHO3D_LOGERROR("Failed to load font image file");
            return false;
        }
        SharedPtr<Texture2D> texture = LoadFaceTexture(fontImage);
        if (!texture)
            return false;

        textures_.Push(texture);

        // Add texture to resource cache
        texture->SetName(fontFile->GetName());
        resourceCache->AddManualResource(texture);

        totalTextureSize += fontImage->GetWidth() * fontImage->GetHeight() * fontImage->GetComponents();

        pageElem = pageElem.GetNext("page");
    }

    XMLElement charsElem = root.GetChild("chars");
    int count = charsElem.GetI32("count");

    XMLElement charElem = charsElem.GetChild("char");
    while (!charElem.IsNull())
    {
        int id = charElem.GetI32("id");

        FontGlyph glyph;
        glyph.x_ = (short)charElem.GetI32("x");
        glyph.y_ = (short)charElem.GetI32("y");
        glyph.width_ = glyph.texWidth_ = (short)charElem.GetI32("width");
        glyph.height_ = glyph.texHeight_ = (short)charElem.GetI32("height");
        glyph.offsetX_ = (short)charElem.GetI32("xoffset");
        glyph.offsetY_ = (short)charElem.GetI32("yoffset");
        glyph.advanceX_ = (short)charElem.GetI32("xadvance");
        glyph.page_ = charElem.GetI32("page");
        assert(glyph.page_ >= 0);

        glyphMapping_[id] = glyph;

        charElem = charElem.GetNext("char");
    }

    XMLElement kerningsElem = root.GetChild("kernings");
    if (kerningsElem.NotNull())
    {
        XMLElement kerningElem = kerningsElem.GetChild("kerning");
        while (!kerningElem.IsNull())
        {
            unsigned first = kerningElem.GetI32("first");
            unsigned second = kerningElem.GetI32("second");
            unsigned value = first << 16u | second;
            kerningMapping_[value] = (short)kerningElem.GetI32("amount");

            kerningElem = kerningElem.GetNext("kerning");
        }
    }

    URHO3D_LOGDEBUGF("Bitmap font face %s has %d glyphs", GetFileName(font_->GetName()).CString(), count);

    font_->SetMemoryUse(font_->GetMemoryUse() + totalTextureSize);
    return true;
}

bool FontFaceBitmap::Load(FontFace* fontFace, bool usedGlyphs)
{
    if (this == fontFace)
        return true;

    if (!usedGlyphs)
    {
        glyphMapping_ = fontFace->glyphMapping_;
        kerningMapping_ = fontFace->kerningMapping_;
        textures_ = fontFace->textures_;
        pointSize_ = fontFace->pointSize_;
        rowHeight_ = fontFace->rowHeight_;

        return true;
    }

    pointSize_ = fontFace->pointSize_;
    rowHeight_ = fontFace->rowHeight_;

    unsigned numPages = 1;
    int maxTextureSize = font_->GetSubsystem<UI>()->GetMaxFontTextureSize();
    AreaAllocator allocator(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, maxTextureSize, maxTextureSize);

    for (auto i = fontFace->glyphMapping_.begin(); i != fontFace->glyphMapping_.end(); ++i)
    {
        FontGlyph fontGlyph = i->second;
        if (!fontGlyph.used_)
            continue;

        int x, y;
        if (!allocator.Allocate((int)fontGlyph.width_ + 1, (int)fontGlyph.height_ + 1, x, y))
        {
            ++numPages;

            allocator = AreaAllocator(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, maxTextureSize, maxTextureSize);
            if (!allocator.Allocate((int)fontGlyph.width_ + 1, (int)fontGlyph.height_ + 1, x, y))
                return false;
        }

        fontGlyph.x_ = (short)x;
        fontGlyph.y_ = (short)y;
        fontGlyph.page_ = numPages - 1;

        glyphMapping_[i->first] = fontGlyph;
    }

    // Assume that format is the same for all textures and that bitmap font type may have more than one component
    unsigned components = ConvertFormatToNumComponents(fontFace->textures_[0]->GetFormat());

    // Save the existing textures as image resources
    Vector<SharedPtr<Image>> oldImages;
    for (unsigned i = 0; i < fontFace->textures_.Size(); ++i)
        oldImages.Push(SaveFaceTexture(fontFace->textures_[i]));

    Vector<SharedPtr<Image>> newImages(numPages);
    for (unsigned i = 0; i < numPages; ++i)
    {
        SharedPtr<Image> image(new Image(font_->GetContext()));

        int width = maxTextureSize;
        int height = maxTextureSize;
        if (i == numPages - 1)
        {
            width = allocator.GetWidth();
            height = allocator.GetHeight();
        }

        image->SetSize(width, height, components);
        memset(image->GetData(), 0, (size_t)width * height * components);

        newImages[i] = image;
    }

    for (auto i = glyphMapping_.begin(); i != glyphMapping_.end(); ++i)
    {
        FontGlyph& newGlyph = i->second;
        const FontGlyph& oldGlyph = fontFace->glyphMapping_[i->first];
        Blit(newImages[newGlyph.page_], (int)newGlyph.x_, (int)newGlyph.y_, (int)newGlyph.width_, (int)newGlyph.height_, oldImages[oldGlyph.page_],
            (int)oldGlyph.x_, (int)oldGlyph.y_, components);
    }

    textures_.Resize(newImages.Size());
    for (unsigned i = 0; i < newImages.Size(); ++i)
        textures_[i] = LoadFaceTexture(newImages[i]);

    for (auto i = fontFace->kerningMapping_.begin(); i != fontFace->kerningMapping_.end(); ++i)
    {
        unsigned first = (i->first) >> 16u;
        unsigned second = (i->first) & 0xffffu;
        if (glyphMapping_.find(first) != glyphMapping_.end() && glyphMapping_.find(second) != glyphMapping_.end())
            kerningMapping_[i->first] = i->second;
    }

    return true;
}

bool FontFaceBitmap::Save(Serializer& dest, int pointSize, const String& indentation)
{
    Context* context = font_->GetContext();

    SharedPtr<XMLFile> xml(new XMLFile(context));
    XMLElement rootElem = xml->CreateRoot("font");

    // Information
    XMLElement childElem = rootElem.CreateChild("info");
    String fileName = GetFileName(font_->GetName());
    childElem.SetAttribute("face", fileName);
    childElem.SetAttribute("size", String(pointSize));

    // Common
    childElem = rootElem.CreateChild("common");
    childElem.SetI32("lineHeight", (int)rowHeight_);
    unsigned pages = textures_.Size();
    childElem.SetU32("pages", pages);

    // Construct the path to store the texture
    String pathName;
    auto* file = dynamic_cast<File*>(&dest);
    if (file)
        // If serialize to file, use the file's path
        pathName = GetPath(file->GetName());
    else
        // Otherwise, use the font resource's path
        pathName = "Data/" + GetPath(font_->GetName());

    // Pages
    childElem = rootElem.CreateChild("pages");
    for (unsigned i = 0; i < pages; ++i)
    {
        XMLElement pageElem = childElem.CreateChild("page");
        pageElem.SetI32("id", i);
        String texFileName = fileName + "_" + String(i) + ".png";
        pageElem.SetAttribute("file", texFileName);

        // Save the font face texture to image file
        SaveFaceTexture(textures_[i], pathName + texFileName);
    }

    // Chars and kernings
    XMLElement charsElem = rootElem.CreateChild("chars");
    unsigned numGlyphs = (unsigned)glyphMapping_.size();
    charsElem.SetI32("count", numGlyphs);

    for (auto i = glyphMapping_.begin(); i != glyphMapping_.end(); ++i)
    {
        // Char
        XMLElement charElem = charsElem.CreateChild("char");
        charElem.SetI32("id", i->first);

        const FontGlyph& glyph = i->second;
        charElem.SetI32("x", (int)glyph.x_);
        charElem.SetI32("y", (int)glyph.y_);
        charElem.SetI32("width", (int)glyph.width_);
        charElem.SetI32("height", (int)glyph.height_);
        charElem.SetI32("xoffset", (int)glyph.offsetX_);
        charElem.SetI32("yoffset", glyph.offsetY_);
        charElem.SetI32("xadvance", glyph.advanceX_);
        charElem.SetI32("page", glyph.page_);
    }

    if (!kerningMapping_.empty())
    {
        XMLElement kerningsElem = rootElem.CreateChild("kernings");
        for (auto i = kerningMapping_.begin(); i != kerningMapping_.end(); ++i)
        {
            XMLElement kerningElem = kerningsElem.CreateChild("kerning");
            kerningElem.SetI32("first", i->first >> 16u);
            kerningElem.SetI32("second", i->first & 0xffffu);
            kerningElem.SetI32("amount", (int)i->second);
        }
    }

    return xml->Save(dest, indentation);
}

unsigned FontFaceBitmap::ConvertFormatToNumComponents(unsigned format)
{
    if (format == Graphics::GetRGBAFormat())
        return 4;
    else if (format == Graphics::GetRGBFormat())
        return 3;
    else if (format == Graphics::GetLuminanceAlphaFormat())
        return 2;
    else
        return 1;
}

SharedPtr<Image> FontFaceBitmap::SaveFaceTexture(Texture2D* texture)
{
    SharedPtr<Image> image(new Image(font_->GetContext()));
    image->SetSize(texture->GetWidth(), texture->GetHeight(), ConvertFormatToNumComponents(texture->GetFormat()));
    if (!texture->GetData(0, image->GetData()))
    {
        URHO3D_LOGERROR("Could not save texture to image resource");
        return SharedPtr<Image>();
    }
    return image;
}

bool FontFaceBitmap::SaveFaceTexture(Texture2D* texture, const String& fileName)
{
    SharedPtr<Image> image = SaveFaceTexture(texture);
    return image ? image->SavePNG(fileName) : false;
}

void FontFaceBitmap::Blit(Image* dest, int x, int y, int width, int height, Image* source, int sourceX, int sourceY, int components)
{
    unsigned char* destData = dest->GetData() + (y * dest->GetWidth() + x) * components;
    unsigned char* sourceData = source->GetData() + (sourceY * source->GetWidth() + sourceX) * components;
    for (int i = 0; i < height; ++i)
    {
        memcpy(destData, sourceData, (size_t)width * components);
        destData += dest->GetWidth() * components;
        sourceData += source->GetWidth() * components;
    }
}

}
