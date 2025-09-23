// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../Core/WorkQueue.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Material.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/Technique.h"
#include "../Graphics/View.h"
#include "../Math/Vector4.h"
#include "../GraphicsAPI/IndexBuffer.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../GraphicsAPI/VertexBuffer.h"
#include "../IO/Log.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"
#include "../Urho2D/Drawable2D.h"
#include "../Urho2D/Renderer2D.h"
#include "../Urho2D/Light2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* blendModeNames[];

static const VertexElements MASK_VERTEX2D = VertexElements::Position | VertexElements::Color | VertexElements::TexCoord1;

ViewBatchInfo2D::ViewBatchInfo2D() :
    vertexBufferUpdateFrameNumber_(0),
    indexCount_(0),
    vertexCount_(0),
    batchUpdatedFrameNumber_(0),
    batchCount_(0)
{
}

Renderer2D::Renderer2D(Context* context) :
    Drawable(context, DrawableTypes::Geometry),
    material_(new Material(context)),
    indexBuffer_(new IndexBuffer(context_)),
    viewMask_(DEFAULT_VIEWMASK)
{
    material_->SetName("Urho2D");

    auto* tech = new Technique(context_);
    Pass* pass = tech->CreatePass("alpha");
    pass->SetVertexShader("Urho2D");
    pass->SetPixelShader("Urho2D");
    // 2.5D: 启用深度写入与深度测试（前向，减少遮挡过绘）
    pass->SetDepthWrite(true);
    pass->SetDepthTestMode(CMP_LESSEQUAL);
    cachedTechniques_[BLEND_REPLACE] = tech;

    material_->SetTechnique(0, tech);
    material_->SetCullMode(CULL_NONE);

    frame_.frameNumber_ = 0;
    SubscribeToEvent(E_BEGINVIEWUPDATE, URHO3D_HANDLER(Renderer2D, HandleBeginViewUpdate));
}

Renderer2D::~Renderer2D() = default;

void Renderer2D::RegisterObject(Context* context)
{
    context->RegisterFactory<Renderer2D>();
}

static inline bool CompareRayQueryResults(const RayQueryResult& lr, const RayQueryResult& rr)
{
    auto* lhs = static_cast<Drawable2D*>(lr.drawable_);
    auto* rhs = static_cast<Drawable2D*>(rr.drawable_);
    if (lhs->GetLayer() != rhs->GetLayer())
        return lhs->GetLayer() > rhs->GetLayer();

    if (lhs->GetOrderInLayer() != rhs->GetOrderInLayer())
        return lhs->GetOrderInLayer() > rhs->GetOrderInLayer();

    return lhs->GetID() > rhs->GetID();
}

void Renderer2D::ProcessRayQuery(const RayOctreeQuery& query, Vector<RayQueryResult>& results)
{
    unsigned resultSize = results.Size();
    for (unsigned i = 0; i < static_cast<unsigned>(drawables_.Size()); ++i)
        {
        if (drawables_[i]->GetViewMask() & query.viewMask_)
            drawables_[i]->ProcessRayQuery(query, results);
    }

    if (results.Size() != resultSize)
        Sort(results.Begin() + resultSize, results.End(), CompareRayQueryResults);
}

void Renderer2D::UpdateBatches(const FrameInfo& frame)
{
    unsigned count = batches_.Size();

    // Update non-thread critical parts of the source batches
    for (unsigned i = 0; i < count; ++i)
    {
        batches_[i].distance_ = 10.0f + (count - i) * 0.001f;
        batches_[i].worldTransform_ = &Matrix3x4::IDENTITY;
    }
}

void Renderer2D::UpdateGeometry(const FrameInfo& frame)
{
    unsigned indexCount = 0;
    for (auto i = viewBatchInfos_.begin(); i != viewBatchInfos_.end(); ++i)
        {
        if (i->second.batchUpdatedFrameNumber_ == frame_.frameNumber_)
            indexCount = Max(indexCount, i->second.indexCount_);
    }

    // Fill index buffer
    if ((u32)indexBuffer_->GetIndexCount() < indexCount || indexBuffer_->IsDataLost())
    {
        bool largeIndices = (indexCount * 4 / 6) > 0xffff;
        indexBuffer_->SetSize(indexCount, largeIndices);

        void* buffer = indexBuffer_->Lock(0, indexCount, true);
        if (buffer)
        {
            unsigned quadCount = indexCount / 6;
            if (largeIndices)
            {
                auto* dest = reinterpret_cast<unsigned*>(buffer);
                for (unsigned i = 0; i < quadCount; ++i)
                {
                    unsigned base = i * 4;
                    dest[0] = base;
                    dest[1] = base + 1;
                    dest[2] = base + 2;
                    dest[3] = base;
                    dest[4] = base + 2;
                    dest[5] = base + 3;
                    dest += 6;
                }
            }
            else
            {
                auto* dest = reinterpret_cast<unsigned short*>(buffer);
                for (unsigned i = 0; i < quadCount; ++i)
                {
                    unsigned base = i * 4;
                    dest[0] = (unsigned short)(base);
                    dest[1] = (unsigned short)(base + 1);
                    dest[2] = (unsigned short)(base + 2);
                    dest[3] = (unsigned short)(base);
                    dest[4] = (unsigned short)(base + 2);
                    dest[5] = (unsigned short)(base + 3);
                    dest += 6;
                }
            }

            indexBuffer_->Unlock();
        }
        else
        {
            URHO3D_LOGERROR("Failed to lock index buffer");
            return;
        }
    }

    Camera* camera = frame.camera_;
    ViewBatchInfo2D& viewBatchInfo = viewBatchInfos_[camera];

    if (viewBatchInfo.vertexBufferUpdateFrameNumber_ != frame_.frameNumber_)
    {
        unsigned vertexCount = viewBatchInfo.vertexCount_;
        VertexBuffer* vertexBuffer = viewBatchInfo.vertexBuffer_;
        if ((u32)vertexBuffer->GetVertexCount() < vertexCount)
            vertexBuffer->SetSize(vertexCount, MASK_VERTEX2D, true);

        if (vertexCount)
        {
            auto* dest = reinterpret_cast<Vertex2D*>(vertexBuffer->Lock(0, vertexCount, true));
            if (dest)
            {
                const Vector<const SourceBatch2D*>& sourceBatches = viewBatchInfo.sourceBatches_;
                for (unsigned b = 0; b < static_cast<unsigned>(sourceBatches.Size()); ++b)
                {
                    const Vector<Vertex2D>& vertices = sourceBatches[b]->vertices_;
                    for (unsigned i = 0; i < vertices.Size(); ++i)
                    {
                        dest[i] = vertices[i];
                        // 2.5D: y→z 映射，利用深度测试实现层级遮挡
                        // 经验系数：0.001f，可按项目世界坐标范围调节
                        dest[i].position_.z_ = -dest[i].position_.y_ * 0.001f;

                        // Light2D 顶点色调制（简版）
                        if (!frameLights_.Empty())
                        {
                            auto decode = [](u32 c){
                                float r = (float)(c & 0xFF) / 255.f;
                                float g = (float)((c >> 8) & 0xFF) / 255.f;
                                float b = (float)((c >> 16) & 0xFF) / 255.f;
                                float a = (float)((c >> 24) & 0xFF) / 255.f;
                                return Color(r,g,b,a);
                            };
                            auto encode = [](const Color& col){
                                u32 r = (u32)Clamp((int)(col.r_ * 255.f), 0, 255);
                                u32 g = (u32)Clamp((int)(col.g_ * 255.f), 0, 255);
                                u32 b = (u32)Clamp((int)(col.b_ * 255.f), 0, 255);
                                u32 a = (u32)Clamp((int)(col.a_ * 255.f), 0, 255);
                                return (a<<24) | (b<<16) | (g<<8) | r;
                            };

                            Color base = decode(dest[i].color_);
                            Vector3 rgb(base.r_, base.g_, base.b_);
                            Vector3 add(0,0,0);

                            Vector3 wp = dest[i].position_;
                            for (Light2D* l : frameLights_)
                            {
                                if (!l) continue;
                                if (l->GetLightType() == Light2D::POINT)
                                {
                                    Vector3 lp = l->GetNode()->GetWorldPosition();
                                    float dx = wp.x_ - lp.x_;
                                    float dy = wp.y_ - lp.y_;
                                    float dist = Sqrt(dx*dx + dy*dy);
                                    float r = Max(l->GetRadius(), 0.0001f);
                                    float att = 1.0f - (dist / r);
                                    if (att > 0.f)
                                    {
                                        float k = l->GetIntensity() * att;
                                        add += Vector3(l->GetColor().r_, l->GetColor().g_, l->GetColor().b_) * k;
                                    }
                                }
                                else
                                {
                                    add += Vector3(l->GetColor().r_, l->GetColor().g_, l->GetColor().b_) * (0.1f * l->GetIntensity());
                                }
                            }
                            add = Vector3(Clamp(add.x_, 0.f, 1.f), Clamp(add.y_, 0.f, 1.f), Clamp(add.z_, 0.f, 1.f));
                            rgb = rgb + add * (Vector3::ONE - rgb);
                            base.r_ = Clamp(rgb.x_, 0.f, 1.f);
                            base.g_ = Clamp(rgb.y_, 0.f, 1.f);
                            base.b_ = Clamp(rgb.z_, 0.f, 1.f);
                            dest[i].color_ = encode(base);
                        }
                    }
                    dest += vertices.Size();
                }

                vertexBuffer->Unlock();
            }
            else
                URHO3D_LOGERROR("Failed to lock vertex buffer");
        }

        viewBatchInfo.vertexBufferUpdateFrameNumber_ = frame_.frameNumber_;
    }
}

UpdateGeometryType Renderer2D::GetUpdateGeometryType()
{
    return UPDATE_MAIN_THREAD;
}

void Renderer2D::AddDrawable(Drawable2D* drawable)
{
    if (!drawable)
        return;

    drawables_.Push(drawable);
}

void Renderer2D::RemoveDrawable(Drawable2D* drawable)
{
    if (!drawable)
        return;

    drawables_.Remove(drawable);
}

Material* Renderer2D::GetMaterial(Texture2D* texture, BlendMode blendMode)
{
    if (!texture)
        return material_;

    auto t = cachedMaterials_.find(texture);
    if (t == cachedMaterials_.end())
    {
        SharedPtr<Material> newMaterial = CreateMaterial(texture, blendMode);
        cachedMaterials_[texture][blendMode] = newMaterial;
        return newMaterial;
    }

    HashMap<int, SharedPtr<Material>>& materials = t->second;
    auto b = materials.find(blendMode);
    if (b != materials.end())
        return b->second;

    SharedPtr<Material> newMaterial = CreateMaterial(texture, blendMode);
    materials[blendMode] = newMaterial;

    return newMaterial;
}

bool Renderer2D::CheckVisibility(Drawable2D* drawable) const
{
    if ((viewMask_ & drawable->GetViewMask()) == 0)
        return false;

    const BoundingBox& box = drawable->GetWorldBoundingBox();
    return frustum_.IsInsideFast(box) != OUTSIDE;
}

void Renderer2D::OnWorldBoundingBoxUpdate()
{
    // Set a large dummy bounding box to ensure the renderer is rendered
    boundingBox_.Define(-M_LARGE_VALUE, M_LARGE_VALUE);
    worldBoundingBox_ = boundingBox_;
}

SharedPtr<Material> Renderer2D::CreateMaterial(Texture2D* texture, BlendMode blendMode)
{
    SharedPtr<Material> newMaterial = material_->Clone();

    auto techIt = cachedTechniques_.find((int)blendMode);
    if (techIt == cachedTechniques_.end())
    {
        SharedPtr<Technique> tech(new Technique(context_));
        Pass* pass = tech->CreatePass("alpha");
        pass->SetVertexShader("Urho2D");
        pass->SetPixelShader("Urho2D");
        // 2.5D: 对 2D 精灵启用深度写入与深度测试（简单演示，透明边缘可能存在轻微伪影）
        pass->SetDepthWrite(true);
        pass->SetDepthTestMode(CMP_LESSEQUAL);
        pass->SetBlendMode(blendMode);
        cachedTechniques_[(int)blendMode] = tech;
        techIt = cachedTechniques_.find((int)blendMode);
    }

    newMaterial->SetTechnique(0, techIt->second.Get());
    newMaterial->SetName(texture->GetName() + "_" + blendModeNames[blendMode]);
    newMaterial->SetTexture(TU_DIFFUSE, texture);

    return newMaterial;
}

void CheckDrawableVisibilityWork(const WorkItem* item, i32 threadIndex)
{
    auto* renderer = reinterpret_cast<Renderer2D*>(item->aux_);
    auto** start = reinterpret_cast<Drawable2D**>(item->start_);
    auto** end = reinterpret_cast<Drawable2D**>(item->end_);

    while (start != end)
    {
        Drawable2D* drawable = *start++;
        if (renderer->CheckVisibility(drawable))
            drawable->MarkInView(renderer->frame_);
    }
}

void Renderer2D::HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginViewUpdate;

    // Check that we are updating the correct scene
    if (GetScene() != eventData[P_SCENE].GetPtr())
        return;

    frame_ = static_cast<View*>(eventData[P_VIEW].GetPtr())->GetFrameInfo();

    URHO3D_PROFILE(UpdateRenderer2D);

    auto* camera = static_cast<Camera*>(eventData[P_CAMERA].GetPtr());
    frustum_ = camera->GetFrustum();
    viewMask_ = camera->GetViewMask();

    // 收集本帧 2D 光源（非持有，仅遍历）
    frameLights_.Clear();
    if (Scene* scene = GetScene())
    {
        Vector<Node*> stack;
        stack.Push(scene);
        while (!stack.Empty())
        {
            Node* n = stack.Back();
            stack.Pop();
            const Vector<SharedPtr<Component>>& comps = n->GetComponents();
            for (const SharedPtr<Component>& c : comps)
            {
                if (auto* l = dynamic_cast<Light2D*>(c.Get()))
                    if (l->IsEnabledEffective())
                        frameLights_.Push(l);
            }
            const Vector<SharedPtr<Node>>& children = n->GetChildren();
            for (const SharedPtr<Node>& ch : children)
                stack.Push(ch.Get());
        }
    }

    // BGFX 后端：将本帧 2D 光源写入 uniforms（便于 lit 技术使用）
    if (auto* graphics = GetSubsystem<Graphics>())
    {
        if (graphics->IsBgfxActive())
        {
            const int maxLights = 8;
            Vector<Vector4> posRange;
            Vector<Vector4> colorInt;
            int n = Min((int)frameLights_.Size(), maxLights);
            posRange.Resize(n);
            colorInt.Resize(n);
            for (int i = 0; i < n; ++i)
            {
                Light2D* l = frameLights_[i];
                if (!l) { posRange[i] = Vector4::ZERO; colorInt[i] = Vector4::ZERO; continue; }
                const Vector3 lp3 = l->GetNode()->GetWorldPosition();
                const float typeVal = (l->GetLightType() == Light2D::POINT) ? 1.0f : 0.0f;
                posRange[i] = Vector4(lp3.x_, lp3.y_, l->GetRadius(), typeVal);
                const Color c = l->GetColor();
                colorInt[i] = Vector4(c.r_, c.g_, c.b_, l->GetIntensity());
            }
            const float ambient = 0.0f; // 暂不提供环境光控制
            graphics->BgfxSet2DLights(posRange, colorInt, n, ambient);
        }
    }

    // Check visibility
    {
        URHO3D_PROFILE(CheckDrawableVisibility);

        auto* queue = GetSubsystem<WorkQueue>();
        unsigned numWorkItems = (unsigned)queue->GetNumThreads() + 1; // Worker threads + main thread
        unsigned drawablesPerItem = drawables_.Size() / Max(numWorkItems, 1u);

        Vector<Drawable2D*>::Iterator start = drawables_.Begin();
        for (unsigned i = 0; i < numWorkItems; ++i)
        {
            SharedPtr<WorkItem> item = queue->GetFreeItem();
            item->priority_ = WI_MAX_PRIORITY;
            item->workFunction_ = CheckDrawableVisibilityWork;
            item->aux_ = this;

            Vector<Drawable2D*>::Iterator end = drawables_.End();
            if (i < numWorkItems - 1 && (unsigned)(end - start) > drawablesPerItem)
                end = start + drawablesPerItem;

            item->start_ = &(*start);
            item->end_ = &(*end);
            queue->AddWorkItem(item);

            start = end;
        }

        queue->Complete(WI_MAX_PRIORITY);
    }

    ViewBatchInfo2D& viewBatchInfo = viewBatchInfos_[camera];

    // Create vertex buffer
    if (!viewBatchInfo.vertexBuffer_)
        viewBatchInfo.vertexBuffer_ = new VertexBuffer(context_);

    UpdateViewBatchInfo(viewBatchInfo, camera);

    // 在 BGFX 后端下，直接用 bgfx 提交 2D 批次并阻止老管线重复绘制
    if (auto* graphics = GetSubsystem<Graphics>())
    {
        if (graphics->IsBgfxActive())
        {
            const Matrix4 proj = camera->GetGPUProjection();
            const Matrix3x4& v3 = camera->GetView();
            const Matrix4 view(
                v3.m00_, v3.m01_, v3.m02_, v3.m03_,
                v3.m10_, v3.m11_, v3.m12_, v3.m13_,
                v3.m20_, v3.m21_, v3.m22_, v3.m23_,
                0.0f,    0.0f,    0.0f,    1.0f
            );
            const Matrix4 mvp = proj * view;

            // 逐批提交（按 material 分组已在 UpdateViewBatchInfo 中完成并排序）
            for (const SourceBatch2D* src : viewBatchInfo.sourceBatches_)
            {
                if (!src || src->vertices_.Empty())
                    continue;

                Texture2D* tex = nullptr;
                if (src->material_)
                    tex = static_cast<Texture2D*>(src->material_->GetTexture(TU_DIFFUSE));

                // Vertex2D 布局为: Vector3 position_, u32 color_, Vector2 uv_，与 BgfxDrawQuads 的 QVertex 对齐
                graphics->BgfxDrawQuads(src->vertices_.Buffer(), src->vertices_.Size(), tex, mvp);
            }

            // 清空批次数，避免旧管线继续绘制
            viewBatchInfo.batchCount_ = 0;
            batches_.Clear();
            return;
        }
    }

    // Go through the drawables to form geometries & batches and calculate the total vertex / index count,
    // but upload the actual vertex data later. The idea is that the View class copies our batch vector to
    // its internal data structures, so we can reuse the batches for each view, provided that unique Geometry
    // objects are used for each view to specify the draw ranges
    batches_.Resize(viewBatchInfo.batchCount_);
    for (unsigned i = 0; i < viewBatchInfo.batchCount_; ++i)
    {
        batches_[i].distance_ = viewBatchInfo.distances_[i];
        batches_[i].material_ = viewBatchInfo.materials_[i];
        batches_[i].geometry_ = viewBatchInfo.geometries_[i];
    }
}

void Renderer2D::GetDrawables(Vector<Drawable2D*>& drawables, Node* node)
{
    if (!node || !node->IsEnabled())
        return;

    const Vector<SharedPtr<Component>>& components = node->GetComponents();
    for (Vector<SharedPtr<Component>>::ConstIterator i = components.Begin(); i != components.End(); ++i)
    {
        auto* drawable = dynamic_cast<Drawable2D*>(i->Get());
        if (drawable && drawable->IsEnabled())
            drawables.Push(drawable);
    }

    const Vector<SharedPtr<Node>>& children = node->GetChildren();
    for (Vector<SharedPtr<Node>>::ConstIterator i = children.Begin(); i != children.End(); ++i)
        GetDrawables(drawables, i->Get());
}

static inline bool CompareSourceBatch2Ds(const SourceBatch2D* lhs, const SourceBatch2D* rhs)
{
    if (lhs->drawOrder_ != rhs->drawOrder_)
        return lhs->drawOrder_ < rhs->drawOrder_;

    if (lhs->distance_ != rhs->distance_)
        return lhs->distance_ > rhs->distance_;

    if (lhs->material_ != rhs->material_)
        return lhs->material_->GetNameHash() < rhs->material_->GetNameHash();

    return lhs < rhs;
}

void Renderer2D::UpdateViewBatchInfo(ViewBatchInfo2D& viewBatchInfo, Camera* camera)
{
    // Already update in same frame
    if (viewBatchInfo.batchUpdatedFrameNumber_ == frame_.frameNumber_)
        return;

    Vector<const SourceBatch2D*>& sourceBatches = viewBatchInfo.sourceBatches_;
    sourceBatches.Clear();
    for (unsigned d = 0; d < static_cast<unsigned>(drawables_.Size()); ++d)
    {
        if (!drawables_[d]->IsInView(camera))
            continue;

        const Vector<SourceBatch2D>& batches = drawables_[d]->GetSourceBatches();

        for (const SourceBatch2D& batch : batches)
        {
            if (batch.material_ && !batch.vertices_.Empty())
                sourceBatches.Push(&batch);
        }
    }

    for (const SourceBatch2D* sourceBatch : sourceBatches)
    {
        Vector3 worldPos = sourceBatch->owner_->GetNode()->GetWorldPosition();
        sourceBatch->distance_ = camera->GetDistance(worldPos);
    }

    Sort(sourceBatches.Begin(), sourceBatches.End(), CompareSourceBatch2Ds);

    viewBatchInfo.batchCount_ = 0;
    Material* currMaterial = nullptr;
    unsigned iStart = 0;
    unsigned iCount = 0;
    unsigned vStart = 0;
    unsigned vCount = 0;
    float distance = M_INFINITY;

    for (unsigned b = 0; b < static_cast<unsigned>(sourceBatches.Size()); ++b)
    {
        distance = Min(distance, sourceBatches[b]->distance_);
        Material* material = sourceBatches[b]->material_;
        const Vector<Vertex2D>& vertices = sourceBatches[b]->vertices_;

        // When new material encountered, finish the current batch and start new
        if (currMaterial != material)
        {
            if (currMaterial)
            {
                AddViewBatch(viewBatchInfo, currMaterial, iStart, iCount, vStart, vCount, distance);
                iStart += iCount;
                iCount = 0;
                vStart += vCount;
                vCount = 0;
                distance = M_INFINITY;
            }

            currMaterial = material;
        }

        iCount += vertices.Size() * 6 / 4;
        vCount += vertices.Size();
    }

    // Add the final batch if necessary
    if (currMaterial && vCount)
        AddViewBatch(viewBatchInfo, currMaterial, iStart, iCount, vStart, vCount,distance);

    viewBatchInfo.indexCount_ = iStart + iCount;
    viewBatchInfo.vertexCount_ = vStart + vCount;
    viewBatchInfo.batchUpdatedFrameNumber_ = frame_.frameNumber_;
}

void Renderer2D::AddViewBatch(ViewBatchInfo2D& viewBatchInfo, Material* material,
    unsigned indexStart, unsigned indexCount, unsigned vertexStart, unsigned vertexCount, float distance)
{
    if (!material || indexCount == 0 || vertexCount == 0)
        return;

    if (viewBatchInfo.distances_.Size() <= viewBatchInfo.batchCount_)
        viewBatchInfo.distances_.Resize(viewBatchInfo.batchCount_ + 1);
    viewBatchInfo.distances_[viewBatchInfo.batchCount_] = distance;

    if (viewBatchInfo.materials_.Size() <= viewBatchInfo.batchCount_)
        viewBatchInfo.materials_.Resize(viewBatchInfo.batchCount_ + 1);
    viewBatchInfo.materials_[viewBatchInfo.batchCount_] = material;

    // Allocate new geometry if necessary
    if (viewBatchInfo.geometries_.Size() <= viewBatchInfo.batchCount_)
    {
        SharedPtr<Geometry> geometry(new Geometry(context_));
        geometry->SetIndexBuffer(indexBuffer_);
        geometry->SetVertexBuffer(0, viewBatchInfo.vertexBuffer_);

        viewBatchInfo.geometries_.Push(geometry);
    }

    Geometry* geometry = viewBatchInfo.geometries_[viewBatchInfo.batchCount_];
    geometry->SetDrawRange(TRIANGLE_LIST, indexStart, indexCount, vertexStart, vertexCount, false);

    viewBatchInfo.batchCount_++;
}

}
