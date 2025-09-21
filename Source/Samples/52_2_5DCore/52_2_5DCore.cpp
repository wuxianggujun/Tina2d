#include "../Sample.h"
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Input/InputEvents.h>
#include <Urho3D/Input/Input.h>
#include "Urho3D/Urho2D/Light2D.h"
#include <Urho3D/Urho2D/Sprite2D.h>
#include <Urho3D/Urho2D/StaticSprite2D.h>

using namespace Urho3D;

// 2.5D 核心示例（占位）：
// 目的：为后续 2.5D 能力（深度、Light2D、法线贴图）提供场景脚手架与运行入口。

class Sample_2_5DCore : public Sample
{
    URHO3D_OBJECT(Sample_2_5DCore, Sample);

public:
    explicit Sample_2_5DCore(Context* context) : Sample(context) { }

    void Start() override
    {
        // 基础 Sample 初始化
        Sample::Start();

        // 创建 2D 场景
        CreateScene();

        // 设置 UI 文本，说明当前示例为占位，后续将逐步增强
        CreateInstructions();

        // 订阅更新事件（如需交互可扩展）
        SubscribeToEvents();
    }

private:
    void CreateScene()
    {
        auto* cache = GetSubsystem<ResourceCache>();

        scene_ = new Scene(context_);
        // 避免模板实例化带来的不完整类型问题：用字符串方式创建 Octree 组件
        scene_->CreateComponent("Octree");

        // 相机
        cameraNode_ = scene_->CreateChild("Camera");
        auto* camera = cameraNode_->CreateComponent<Camera>();
        camera->SetOrthographic(true);
        // 让相机位于世界前方，便于观察 z=0 的 2D 内容
        cameraNode_->SetPosition(Vector3(0.f, 0.f, -10.f));
        // 以屏幕像素高度为正交大小，坐标与像素近似一致
        auto* graphics = GetSubsystem<Graphics>();
        camera->SetOrthoSize((float)graphics->GetHeight() * PIXEL_SIZE);

        // 视口
        SharedPtr<Viewport> viewport(new Viewport(context_, scene_, camera));
        GetSubsystem<Renderer>()->SetViewport(0, viewport);

        // 使用真实 2D 精灵：Aster.png（随 Urho2D 资源提供）
        Sprite2D* sprite = cache->GetResource<Sprite2D>("Urho2D/Aster.png");
        if (sprite)
        {
            // 下方精灵（y 小，按 y→z 映射得到更靠前的 z）
            Node* lower = scene_->CreateChild("LowerSprite");
            lower->SetPosition(Vector3(-1.0f, -2.0f, 0.0f));
            auto* ss0 = lower->CreateComponent<StaticSprite2D>();
            ss0->SetSprite(sprite);

            // 上方精灵（y 大，z 更靠后）
            Node* upper = scene_->CreateChild("UpperSprite");
            // 轻微重叠到下方精灵，直观看到遮挡关系（下方会挡住上方重叠部分）
            upper->SetPosition(Vector3(-0.7f, -1.6f, 0.0f));
            auto* ss1 = upper->CreateComponent<StaticSprite2D>();
            ss1->SetSprite(sprite);
            // 染色区分
            ss1->SetColor(Color(0.7f, 0.9f, 1.0f));

            // 第三朵，做上下往返运动，动态展示遮挡切换
            mover_ = scene_->CreateChild("MoverSprite");
            mover_->SetPosition(Vector3(0.8f, -2.5f, 0.0f));
            auto* ssm = mover_->CreateComponent<StaticSprite2D>();
            ssm->SetSprite(sprite);
            ssm->SetColor(Color(1.0f, 0.8f, 0.8f));

            // 添加 Light2D：一个方向光 + 一个点光（点光随时间移动）
            Node* dirNode = scene_->CreateChild("DirLight2D");
            auto* dirL = dirNode->CreateComponent<Light2D>();
            dirL->SetLightType(Light2D::DIRECTIONAL);
            dirL->SetColor(Color(0.8f, 0.8f, 0.9f));
            dirL->SetIntensity(0.6f);

            pointLightNode_ = scene_->CreateChild("PointLight2D");
            pointLightNode_->SetPosition(Vector3(0.5f, 0.0f, 0.0f));
            auto* ptL = pointLightNode_->CreateComponent<Light2D>();
            ptL->SetLightType(Light2D::POINT);
            ptL->SetRadius(2.5f);
            ptL->SetIntensity(0.9f);
            ptL->SetColor(Color(1.0f, 0.6f, 0.4f));
        }
    }

    void CreateInstructions()
    {
        auto* cache = GetSubsystem<ResourceCache>();
        auto* ui = GetSubsystem<UI>();

        // 说明文本
        SharedPtr<Text> text(new Text(context_));
        text->SetText(
            "2.5D 核心示例（占位）\n"
            "- 当前演示场景脚手架，后续迭代将逐步加入：\n"
            "  1) 2D 深度写/深度测 + y→z 映射\n"
            "  2) Light2D 轻量光照\n"
            "  3) 法线贴图与低成本阴影\n"
            "操作：空格 暂停/继续移动\n"
        );
        // 使用按名称设置字体，避免模板 GetResource<Font>() 的不完整类型实例化
        text->SetFont("Fonts/Anonymous Pro.ttf", 14);
        text->SetHorizontalAlignment(HA_LEFT);
        text->SetVerticalAlignment(VA_TOP);
        text->SetColor(Color::CYAN);

        ui->GetRoot()->AddChild(text.Get());
    }

    void SubscribeToEvents()
    {
        // 帧更新：驱动第三朵花上下往返
        SubscribeToEvent(E_UPDATE, [this](StringHash, VariantMap& eventData){
            if (!mover_ || paused_)
                return;
            using namespace Update;
            float dt = eventData[P_TIMESTEP].GetFloat();
            Vector3 pos = mover_->GetPosition();
            pos.y_ += moverSpeed_ * dt;
            // 在区间 [-3.0, 3.0] 内往返
            if (pos.y_ > 3.0f) { pos.y_ = 3.0f; moverSpeed_ = -fabsf(moverSpeed_); }
            if (pos.y_ < -3.0f){ pos.y_ = -3.0f; moverSpeed_ =  fabsf(moverSpeed_); }
            mover_->SetPosition(pos);

            // 点光左右小幅摆动
            if (pointLightNode_)
            {
                Vector3 lp = pointLightNode_->GetPosition();
                phase_ += dt;
                lp.x_ = 0.5f + 0.8f * Sin(phase_ * 1.2f);
                pointLightNode_->SetPosition(lp);
            }
        });

        // 键盘：空格 暂停/继续
        SubscribeToEvent(E_KEYDOWN, [this](StringHash, VariantMap& ev){
            using namespace KeyDown;
            int key = ev[P_KEY].GetI32();
            if (key == KEY_SPACE)
                paused_ = !paused_;
        });
    }

private:
    SharedPtr<Node> mover_;
    float moverSpeed_ { 1.2f };
    bool paused_ { false };
    SharedPtr<Node> pointLightNode_;
    float phase_ { 0.0f };
};

URHO3D_DEFINE_APPLICATION_MAIN(Sample_2_5DCore)
