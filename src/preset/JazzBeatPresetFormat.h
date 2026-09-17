#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace dtjb::preset {

static constexpr const char* kExtension = ".dtjbpreset";
static constexpr std::uint32_t kContainerVersion = 1u;
static constexpr std::uint32_t kPayloadVersion = 1u;
static constexpr std::uint32_t kHashAlgorithmSHA3_256 = 0x00030001u;
static constexpr std::size_t kHeaderBytes = 80u;
static constexpr std::size_t kEntryBytes = 16u;
static constexpr std::size_t kMaxFileBytes = 64u * 1024u;
static constexpr std::uint32_t kMaxEntries = 64u;

static constexpr std::array<std::uint8_t, 8> kMagic {{
  'D','T','J','B','P','R','S','1'
}};
static constexpr std::array<std::uint8_t, 8> kProduct {{
  'J','A','Z','Z','B','E','A','T'
}};

enum class ValueType : std::uint32_t {
  Continuous = 1u,
  Toggle = 2u,
  Enumeration = 3u
};

struct Entry {
  std::uint32_t id = 0u;
  ValueType type = ValueType::Continuous;
  double value = 0.0;
};

enum class ParseError {
  None,
  TooSmall,
  TooLarge,
  BadMagic,
  WrongProduct,
  UnsupportedContainerVersion,
  UnsupportedPayloadVersion,
  UnsupportedHashAlgorithm,
  BadEntryCount,
  BadPayloadSize,
  BadDigest,
  DuplicateId,
  InvalidValueType,
  NonFiniteValue
};

inline const char* ParseErrorText(ParseError error) {
  switch (error) {
    case ParseError::None: return "OK";
    case ParseError::TooSmall: return "Preset file is truncated";
    case ParseError::TooLarge: return "Preset file is too large";
    case ParseError::BadMagic: return "File is not a Der Tondehr Jazz Beat preset";
    case ParseError::WrongProduct: return "Preset belongs to a different Der Tondehr product";
    case ParseError::UnsupportedContainerVersion: return "Unsupported Jazz Beat preset container version";
    case ParseError::UnsupportedPayloadVersion: return "Preset was created by a newer Jazz Beat preset format";
    case ParseError::UnsupportedHashAlgorithm: return "Unsupported Jazz Beat preset integrity algorithm";
    case ParseError::BadEntryCount: return "Preset contains an invalid control count";
    case ParseError::BadPayloadSize: return "Preset payload size is invalid";
    case ParseError::BadDigest: return "Jazz Beat preset integrity check failed";
    case ParseError::DuplicateId: return "Preset contains duplicate control IDs";
    case ParseError::InvalidValueType: return "Preset contains an invalid control type";
    case ParseError::NonFiniteValue: return "Preset contains an invalid control value";
  }
  return "Invalid Jazz Beat preset";
}

namespace detail {

inline std::uint64_t RotL64(std::uint64_t x, unsigned int n) {
  return n == 0u ? x : ((x << n) | (x >> (64u - n)));
}

class SHA3_256 {
public:
  SHA3_256() { Reset(); }

  void Reset() {
    mState.fill(0u);
    mBuffer.fill(0u);
    mBufferSize = 0u;
  }

  void Update(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (!bytes || size == 0u) return;
    while (size > 0u) {
      const std::size_t take = std::min(kRateBytes - mBufferSize, size);
      std::memcpy(mBuffer.data() + mBufferSize, bytes, take);
      mBufferSize += take;
      bytes += take;
      size -= take;
      if (mBufferSize == kRateBytes) {
        AbsorbBlock(mBuffer.data());
        mBuffer.fill(0u);
        mBufferSize = 0u;
      }
    }
  }

  std::array<std::uint8_t, 32> Final() {
    // SHA-3 domain suffix = 0x06, followed by the multi-rate 0x80 terminator.
    mBuffer[mBufferSize] ^= 0x06u;
    mBuffer[kRateBytes - 1u] ^= 0x80u;
    AbsorbBlock(mBuffer.data());

    std::array<std::uint8_t, 32> out {};
    for (std::size_t i = 0; i < out.size(); ++i)
      out[i] = static_cast<std::uint8_t>((mState[i / 8u] >> ((i % 8u) * 8u)) & 0xffu);
    return out;
  }

private:
  static constexpr std::size_t kRateBytes = 136u; // 1088-bit rate for SHA3-256

  static std::uint64_t ReadLE64(const std::uint8_t* p) {
    std::uint64_t v = 0u;
    for (int i = 0; i < 8; ++i)
      v |= static_cast<std::uint64_t>(p[i]) << (8u * static_cast<unsigned int>(i));
    return v;
  }

  void AbsorbBlock(const std::uint8_t* block) {
    for (std::size_t i = 0; i < kRateBytes / 8u; ++i)
      mState[i] ^= ReadLE64(block + i * 8u);
    Permute();
  }

  void Permute() {
    static constexpr std::uint64_t rc[24] = {
      0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
      0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
      0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
      0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
      0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
      0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL
    };
    static constexpr unsigned int rho[25] = {
       0, 1,62,28,27,
      36,44, 6,55,20,
       3,10,43,25,39,
      41,45,15,21, 8,
      18, 2,61,56,14
    };

    for (int round = 0; round < 24; ++round) {
      std::uint64_t c[5] {}, d[5] {}, b[25] {};
      for (int x = 0; x < 5; ++x)
        c[x] = mState[x] ^ mState[x + 5] ^ mState[x + 10] ^ mState[x + 15] ^ mState[x + 20];
      for (int x = 0; x < 5; ++x)
        d[x] = c[(x + 4) % 5] ^ RotL64(c[(x + 1) % 5], 1);
      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
          mState[x + 5 * y] ^= d[x];

      // Rho + Pi. Destination coordinates: (y, 2x + 3y).
      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
          const int dstX = y;
          const int dstY = (2 * x + 3 * y) % 5;
          b[dstX + 5 * dstY] = RotL64(mState[x + 5 * y], rho[x + 5 * y]);
        }

      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
          mState[x + 5 * y] = b[x + 5 * y] ^ ((~b[((x + 1) % 5) + 5 * y]) & b[((x + 2) % 5) + 5 * y]);

      mState[0] ^= rc[round];
    }
  }

  std::array<std::uint64_t, 25> mState {};
  std::array<std::uint8_t, kRateBytes> mBuffer {};
  std::size_t mBufferSize = 0u;
};

inline void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu));
}
inline void AppendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (int i = 0; i < 8; ++i)
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu));
}
inline void AppendDouble(std::vector<std::uint8_t>& out, double value) {
  static_assert(sizeof(double) == sizeof(std::uint64_t), "Jazz Beat presets require IEEE-754 64-bit double");
  std::uint64_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(bits));
  AppendU64(out, bits);
}
inline bool ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t& pos, std::uint32_t& value) {
  if (pos + 4u > bytes.size()) return false;
  value = 0u;
  for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[pos++]) << (i * 8);
  return true;
}
inline bool ReadU64(const std::vector<std::uint8_t>& bytes, std::size_t& pos, std::uint64_t& value) {
  if (pos + 8u > bytes.size()) return false;
  value = 0u;
  for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(bytes[pos++]) << (i * 8);
  return true;
}
inline bool ReadDouble(const std::vector<std::uint8_t>& bytes, std::size_t& pos, double& value) {
  std::uint64_t bits = 0u;
  if (!ReadU64(bytes, pos, bits)) return false;
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}
inline void FeedU32(SHA3_256& hash, std::uint32_t value) {
  std::uint8_t bytes[4] {};
  for (int i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu);
  hash.Update(bytes, sizeof(bytes));
}
inline void FeedU64(SHA3_256& hash, std::uint64_t value) {
  std::uint8_t bytes[8] {};
  for (int i = 0; i < 8; ++i) bytes[i] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu);
  hash.Update(bytes, sizeof(bytes));
}

inline std::array<std::uint8_t, 32> ComputeDigest(std::uint32_t containerVersion,
                                                  std::uint32_t payloadVersion,
                                                  std::uint32_t entryCount,
                                                  std::uint32_t hashAlgorithm,
                                                  std::uint64_t payloadSize,
                                                  const std::uint8_t* payload,
                                                  std::size_t payloadBytes) {
  static constexpr std::uint8_t domain[] = {
    'D','e','r','T','o','n','d','e','h','r','J','a','z','z','B','e','a','t','P','r','e','s','e','t','-','v','1','\0'
  };
  SHA3_256 hash;
  hash.Update(domain, sizeof(domain));
  hash.Update(kMagic.data(), kMagic.size());
  hash.Update(kProduct.data(), kProduct.size());
  FeedU32(hash, containerVersion);
  FeedU32(hash, payloadVersion);
  FeedU32(hash, entryCount);
  FeedU32(hash, hashAlgorithm);
  FeedU64(hash, payloadSize);
  if (payload && payloadBytes) hash.Update(payload, payloadBytes);
  return hash.Final();
}

} // namespace detail

inline std::array<std::uint8_t, 32> SHA3_256ForTesting(const void* data, std::size_t size) {
  detail::SHA3_256 hash;
  hash.Update(data, size);
  return hash.Final();
}

inline bool BuildFile(const std::vector<Entry>& entries, std::vector<std::uint8_t>& bytes) {
  bytes.clear();
  if (entries.empty() || entries.size() > kMaxEntries) return false;

  std::unordered_set<std::uint32_t> ids;
  std::vector<std::uint8_t> payload;
  payload.reserve(entries.size() * kEntryBytes);
  for (const Entry& entry : entries) {
    if (entry.id == 0u || !ids.insert(entry.id).second || !std::isfinite(entry.value)) return false;
    const std::uint32_t type = static_cast<std::uint32_t>(entry.type);
    if (type < static_cast<std::uint32_t>(ValueType::Continuous)
        || type > static_cast<std::uint32_t>(ValueType::Enumeration)) return false;
    detail::AppendU32(payload, entry.id);
    detail::AppendU32(payload, type);
    detail::AppendDouble(payload, entry.value);
  }

  const std::uint64_t payloadSize = static_cast<std::uint64_t>(payload.size());
  if (kHeaderBytes + payload.size() > kMaxFileBytes) return false;
  const auto digest = detail::ComputeDigest(kContainerVersion, kPayloadVersion,
                                             static_cast<std::uint32_t>(entries.size()),
                                             kHashAlgorithmSHA3_256, payloadSize,
                                             payload.data(), payload.size());

  bytes.reserve(kHeaderBytes + payload.size());
  bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
  bytes.insert(bytes.end(), kProduct.begin(), kProduct.end());
  detail::AppendU32(bytes, kContainerVersion);
  detail::AppendU32(bytes, kPayloadVersion);
  detail::AppendU32(bytes, static_cast<std::uint32_t>(entries.size()));
  detail::AppendU32(bytes, kHashAlgorithmSHA3_256);
  detail::AppendU64(bytes, payloadSize);
  bytes.insert(bytes.end(), digest.begin(), digest.end());
  detail::AppendU64(bytes, 0u);
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes.size() == kHeaderBytes + payload.size();
}

inline bool ParseFile(const std::vector<std::uint8_t>& bytes,
                      std::vector<Entry>& entries,
                      ParseError& error) {
  entries.clear();
  error = ParseError::None;
  if (bytes.size() < kHeaderBytes) { error = ParseError::TooSmall; return false; }
  if (bytes.size() > kMaxFileBytes) { error = ParseError::TooLarge; return false; }
  if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) { error = ParseError::BadMagic; return false; }
  if (!std::equal(kProduct.begin(), kProduct.end(), bytes.begin() + 8)) { error = ParseError::WrongProduct; return false; }

  std::size_t pos = 16u;
  std::uint32_t containerVersion = 0u, payloadVersion = 0u, entryCount = 0u, hashAlgorithm = 0u;
  std::uint64_t payloadSize = 0u;
  if (!detail::ReadU32(bytes, pos, containerVersion)
      || !detail::ReadU32(bytes, pos, payloadVersion)
      || !detail::ReadU32(bytes, pos, entryCount)
      || !detail::ReadU32(bytes, pos, hashAlgorithm)
      || !detail::ReadU64(bytes, pos, payloadSize)) {
    error = ParseError::TooSmall; return false;
  }
  if (containerVersion != kContainerVersion) { error = ParseError::UnsupportedContainerVersion; return false; }
  if (payloadVersion == 0u || payloadVersion > kPayloadVersion) { error = ParseError::UnsupportedPayloadVersion; return false; }
  if (hashAlgorithm != kHashAlgorithmSHA3_256) { error = ParseError::UnsupportedHashAlgorithm; return false; }
  if (entryCount == 0u || entryCount > kMaxEntries) { error = ParseError::BadEntryCount; return false; }
  if (payloadSize != static_cast<std::uint64_t>(entryCount) * kEntryBytes
      || payloadSize != static_cast<std::uint64_t>(bytes.size() - kHeaderBytes)) {
    error = ParseError::BadPayloadSize; return false;
  }

  std::array<std::uint8_t, 32> storedDigest {};
  std::memcpy(storedDigest.data(), bytes.data() + pos, storedDigest.size());
  pos += storedDigest.size();
  std::uint64_t reserved = 0u;
  if (!detail::ReadU64(bytes, pos, reserved) || pos != kHeaderBytes) { error = ParseError::TooSmall; return false; }
  if (reserved != 0u) { error = ParseError::UnsupportedContainerVersion; return false; }

  const auto computed = detail::ComputeDigest(containerVersion, payloadVersion, entryCount,
                                               hashAlgorithm, payloadSize,
                                               bytes.data() + kHeaderBytes,
                                               static_cast<std::size_t>(payloadSize));
  if (computed != storedDigest) { error = ParseError::BadDigest; return false; }

  entries.reserve(entryCount);
  std::unordered_set<std::uint32_t> ids;
  pos = kHeaderBytes;
  for (std::uint32_t i = 0; i < entryCount; ++i) {
    std::uint32_t id = 0u, typeRaw = 0u;
    double value = 0.0;
    if (!detail::ReadU32(bytes, pos, id) || !detail::ReadU32(bytes, pos, typeRaw)
        || !detail::ReadDouble(bytes, pos, value)) { error = ParseError::BadPayloadSize; return false; }
    if (id == 0u || !ids.insert(id).second) { error = ParseError::DuplicateId; return false; }
    if (typeRaw < static_cast<std::uint32_t>(ValueType::Continuous)
        || typeRaw > static_cast<std::uint32_t>(ValueType::Enumeration)) { error = ParseError::InvalidValueType; return false; }
    if (!std::isfinite(value)) { error = ParseError::NonFiniteValue; return false; }
    entries.push_back({id, static_cast<ValueType>(typeRaw), value});
  }
  return pos == bytes.size();
}

} // namespace dtjb::preset
