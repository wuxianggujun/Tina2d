// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/IOEvents.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/ListView.h>
#include <Urho3D/UI/Text.h>

#include "LogPanel.h"

namespace Urho3D
{

LogPanel::LogPanel(Context* context, UIElement* parent)
    : Object(context)
{
    list_ = parent->CreateChild<ListView>("LogList");
    list_->SetStyleAuto();
    list_->SetSelectOnClickEnd(true);

    SubscribeToEvent(E_LOGMESSAGE, URHO3D_HANDLER(LogPanel, HandleLogMessage));
}

void LogPanel::HandleLogMessage(StringHash, VariantMap& data)
{
    using namespace LogMessage;
    const String& msg = data[P_MESSAGE].GetString();
    const int level = data[P_LEVEL].GetI32();

    auto* row = new Text(context_);
    row->SetStyleAuto();
    row->SetText(msg);
    if (level >= LOG_ERROR)
        row->SetColor(Color(1, 0.3f, 0.3f));
    else if (level == LOG_WARNING)
        row->SetColor(Color(1, 0.8f, 0.3f));

    list_->AddItem(row);
    list_->EnsureItemVisibility(row);
}

} // namespace Urho3D
