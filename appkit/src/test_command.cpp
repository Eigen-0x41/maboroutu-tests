// maboroutu.command のテスト。
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.command;

namespace {

static_assert(std::is_abstract_v<maboroutu::command>);
static_assert(std::has_virtual_destructor_v<maboroutu::command>);
static_assert(!std::is_constructible_v<maboroutu::command>);

// 名前の変更を表す、利用側の派生型の例。
class rename_command final : public maboroutu::command {
 public:
   rename_command(std::string &target, std::string next)
       : _target(target), _previous(target), _next(std::move(next)) {}
   void execute() override { _target = _next; }
   void undo() override { _target = _previous; }

 private:
   std::string &_target;
   std::string _previous;
   std::string _next;
};

class counting_command final : public maboroutu::command {
 public:
   counting_command(int &destroyed) : _destroyed(destroyed) {}
   ~counting_command() override { ++_destroyed; }
   void execute() override {}
   void undo() override {}

 private:
   int &_destroyed;
};

TEST(MaboroutuCommand, ExecuteThenUndoRestoresState) {
   std::string name = "before";
   std::unique_ptr<maboroutu::command> cmd =
       std::make_unique<rename_command>(name, "after");
   cmd->execute();
   EXPECT_EQ(name, "after");
   cmd->undo();
   EXPECT_EQ(name, "before");
}

TEST(MaboroutuCommand, ExecuteAgainAfterUndoRedoes) {
   std::string name = "a";
   rename_command cmd(name, "b");
   maboroutu::command &base = cmd;
   base.execute();
   base.undo();
   base.execute();
   EXPECT_EQ(name, "b");
}

// 利用側が、履歴を base 型のポインタで保持できること。
TEST(MaboroutuCommand, UndoHistoryInReverseOrder) {
   std::string name = "0";
   std::vector<std::unique_ptr<maboroutu::command>> history;
   for (auto next : {"1", "2", "3"}) {
      history.push_back(std::make_unique<rename_command>(name, next));
      history.back()->execute();
   }
   EXPECT_EQ(name, "3");
   // 各 rename_command は構築時の値を保持するので、逆順に undo すると元へ戻る。
   while (!history.empty()) {
      history.back()->undo();
      history.pop_back();
   }
   EXPECT_EQ(name, "0");
}

TEST(MaboroutuCommand, DeleteThroughBaseDestroysDerived) {
   int destroyed = 0;
   {
      std::unique_ptr<maboroutu::command> cmd =
          std::make_unique<counting_command>(destroyed);
   }
   EXPECT_EQ(destroyed, 1);
}

} // namespace
