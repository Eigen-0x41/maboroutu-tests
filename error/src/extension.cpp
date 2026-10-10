// extension を -fno-exceptions でビルド・実行できることの確認（abort 構成）。
// 例外が無効な場合、routed_emplace の失敗は重複key（failed_to_add_key）のみ。
#include <cstddef>
#include <cstdlib>
#include <string>
import maboroutu.slot_map;
import maboroutu.keyed_slot_map;

namespace {
enum class idx_t : std::size_t {};
using underlying_map = maboroutu::slot_map<idx_t, int>;
using keyed_map = maboroutu::keyed_slot_map<std::string, underlying_map>;
using code_type = maboroutu::keyed_slot_map_result<void>::error_type::code_type;
} // namespace

auto main() -> int {
  // slot_map::emplace（MSVCで偽評価された can_emplace_back の経路）
  underlying_map plain;
  auto const first = plain.emplace(7);
  auto const second = plain.emplace(8);
  if (plain.size() != 2 || plain[first] != 7 || plain.at(second) != 8) {
    return 1;
  }
  plain.erase(first);
  if (plain.contains(first) || plain.size() != 1) {
    return 2;
  }

  underlying_map slots;
  keyed_map keyed(slots);
  if (!keyed.routed_emplace(std::string("alpha"), 42)) {
    return 3;
  }
  if (!keyed.contains(std::string("alpha"))) {
    return 4;
  }
  if (keyed.routed_at(std::string("alpha")) != 42) {
    return 5;
  }

  auto const dup = keyed.routed_emplace(std::string("alpha"), 1);
  if (dup.has_value() || dup.error().code() != code_type::failed_to_add_key) {
    return 6;
  }
  if (keyed.routed_at(std::string("alpha")) != 42) {
    return 7;
  }

  auto const missing = keyed.require_routed_exist(std::string("beta"));
  if (missing.has_value() ||
      missing.error().code() != code_type::key_was_not_contain) {
    return 8;
  }

  if (!keyed.routed_erase(std::string("alpha"))) {
    return 9;
  }
  if (keyed.contains(std::string("alpha"))) {
    return 10;
  }
  return EXIT_SUCCESS;
}
