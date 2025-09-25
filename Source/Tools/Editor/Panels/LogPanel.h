// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

class UIElement;
class ListView;

/// 日志面板：订阅 E_LOGMESSAGE 并显示到 ListView
class LogPanel final : public Object
{
    URHO3D_OBJECT(LogPanel, Object);
public:
    LogPanel(Context* context, UIElement* parent);

private:
    void HandleLogMessage(StringHash, VariantMap&);
    ListView* list_{};
};

} // namespace Urho3D

