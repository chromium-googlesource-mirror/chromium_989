// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

using aura::Window;

class WorkspaceControllerTest : public aura::test::AuraTestBase {
 public:
  WorkspaceControllerTest() {}
  virtual ~WorkspaceControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    contents_view_ = aura::RootWindow::GetInstance();
    // Activatable windows need to be in a container the ActivationController
    // recognizes.
    contents_view_->set_id(
        ash::internal::kShellWindowId_DefaultContainer);
    activation_controller_.reset(new ActivationController);
    activation_controller_->set_default_container_for_test(contents_view_);
    controller_.reset(new WorkspaceController(contents_view_));
  }

  virtual void TearDown() OVERRIDE {
    activation_controller_.reset();
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    contents_view()->AddChild(window);
    window->Show();
    return window;
  }

  aura::Window* contents_view() {
    return contents_view_;
  }

  WorkspaceManager* workspace_manager() {
    return controller_->workspace_manager();
  }

  scoped_ptr<WorkspaceController> controller_;

 private:
  aura::Window* contents_view_;

  scoped_ptr<ActivationController> activation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceControllerTest);
};

TEST_F(WorkspaceControllerTest, Overview) {
  workspace_manager()->SetWorkspaceSize(gfx::Size(500, 300));

  // Creating two workspaces, ws1 which contains window w1,
  // and ws2 which contains window w2.
  Workspace* ws1 = workspace_manager()->CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());
  EXPECT_TRUE(ws1->AddWindowAfter(w1.get(), NULL));

  Workspace* ws2 = workspace_manager()->CreateWorkspace();
  scoped_ptr<Window> w2(CreateTestWindow());
  EXPECT_TRUE(ws2->AddWindowAfter(w2.get(), NULL));

  // Activating a window switches the active workspace.
  ash::ActivateWindow(w2.get());
  EXPECT_EQ(ws2, workspace_manager()->GetActiveWorkspace());

  // The size of contents_view() is now ws1(500) + ws2(500) + margin(50).
  EXPECT_EQ("0,0 1050x300", contents_view()->bounds().ToString());
  EXPECT_FALSE(workspace_manager()->is_overview());
  workspace_manager()->SetOverview(true);
  EXPECT_TRUE(workspace_manager()->is_overview());

  // Switching overview mode doesn't change the active workspace.
  EXPECT_EQ(ws2, workspace_manager()->GetActiveWorkspace());

  // Activating window w1 switches the active window and
  // the mode back to normal mode.
  ash::ActivateWindow(w1.get());
  EXPECT_EQ(ws1, workspace_manager()->GetActiveWorkspace());
  EXPECT_FALSE(workspace_manager()->is_overview());

  // Deleting w1 without StackingClient resets the active workspace
  ws1->RemoveWindow(w1.get());
  delete ws1;
  w1.reset();
  EXPECT_EQ(ws2, workspace_manager()->GetActiveWorkspace());
  EXPECT_EQ("0,0 500x300", contents_view()->bounds().ToString());
  ws2->RemoveWindow(w2.get());
  delete ws2;
  // The size of contents_view() for no workspace case must be
  // same as one contents_view() case.
  EXPECT_EQ("0,0 500x300", contents_view()->bounds().ToString());

  // Reset now before windows are destroyed.
  controller_.reset();
}

}  // namespace internal
}  // namespace ash
