#include "preset/JazzBeatPresetFormat.h"
#include "preset/JazzBeatPresetMap.h"

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::string Hex(const std::array<std::uint8_t, 32>& digest)
{
  static constexpr char h[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (const auto b : digest) {
    out.push_back(h[b >> 4]);
    out.push_back(h[b & 0x0f]);
  }
  return out;
}

int main(int argc, char** argv)
{
  assert(Hex(dtjb::preset::SHA3_256ForTesting("", 0)) ==
         "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
  assert(Hex(dtjb::preset::SHA3_256ForTesting("abc", 3)) ==
         "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");

  assert(dtjb::preset::kParamSpecs.size() == 15u);
  assert(!dtjb::preset::ContainsParamIndex(kHiTrebleLegacy));
  assert(dtjb::preset::ContainsParamIndex(kHiTreble));

  std::vector<dtjb::preset::Entry> source;
  source.reserve(dtjb::preset::kParamSpecs.size());
  for (const auto& spec : dtjb::preset::kParamSpecs) {
    double value = spec.minValue;
    if (spec.type == dtjb::preset::ValueType::Continuous)
      value = (spec.minValue + spec.maxValue) * 0.5;
    source.push_back({spec.stableId, spec.type, value});
  }

  std::vector<std::uint8_t> bytes;
  assert(dtjb::preset::BuildFile(source, bytes));
  assert(bytes.size() == dtjb::preset::kHeaderBytes + source.size() * dtjb::preset::kEntryBytes);

  std::vector<dtjb::preset::Entry> decoded;
  dtjb::preset::ParseError error {};
  assert(dtjb::preset::ParseFile(bytes, decoded, error));
  assert(error == dtjb::preset::ParseError::None);
  assert(decoded.size() == source.size());

  auto tampered = bytes;
  tampered.back() ^= 0x01u;
  assert(!dtjb::preset::ParseFile(tampered, decoded, error));
  assert(error == dtjb::preset::ParseError::BadDigest);

  auto wrongMagic = bytes;
  wrongMagic[0] = 'X';
  assert(!dtjb::preset::ParseFile(wrongMagic, decoded, error));
  assert(error == dtjb::preset::ParseError::BadMagic);

  auto wrongProduct = bytes;
  wrongProduct[8] = 'X';
  assert(!dtjb::preset::ParseFile(wrongProduct, decoded, error));
  assert(error == dtjb::preset::ParseError::WrongProduct);

  auto wrongHash = bytes;
  wrongHash[28] ^= 0x55u;
  assert(!dtjb::preset::ParseFile(wrongHash, decoded, error));
  assert(error == dtjb::preset::ParseError::UnsupportedHashAlgorithm);

  auto duplicate = source;
  duplicate.push_back(source.front());
  assert(!dtjb::preset::BuildFile(duplicate, bytes));

  // Every preset shipped in the public presets/ directory must be a complete,
  // valid Jazz Beat file whose stable IDs, types and ranges match this build.
  for(int arg = 1; arg < argc; ++arg) {
    std::ifstream file(argv[arg], std::ios::binary);
    assert(file.good());
    const std::vector<std::uint8_t> presetBytes {
      std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
    };
    assert(dtjb::preset::ParseFile(presetBytes, decoded, error));
    assert(decoded.size() == dtjb::preset::kParamSpecs.size());
    for(const auto& entry : decoded) {
      const auto* spec = dtjb::preset::FindSpec(entry.id);
      assert(spec != nullptr);
      assert(spec->type == entry.type);
      assert(dtjb::preset::ValueValid(*spec, entry.value));
    }
  }

  std::cout << "Jazz Beat .dtjbpreset format tests passed\n";
  return 0;
}
