#pragma once

#include "bmcweb_config.h"

#include <openssl/crypto.h>

#include <boost/callable_traits.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace crow
{
namespace utility
{

enum class TypeCode : uint8_t
{
    Unspecified = 0,
    String = 1,
    Path = 2,
    Max = 3,
};

// Remove when we have c++23
template <typename E>
constexpr typename std::underlying_type<E>::type toUnderlying(E e) noexcept
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

constexpr inline uint64_t getParameterTag(std::string_view url)
{
    uint64_t tagValue = 0;
    size_t urlSegmentIndex = std::string_view::npos;

    size_t paramIndex = 0;

    for (size_t urlIndex = 0; urlIndex < url.size(); urlIndex++)
    {
        char character = url[urlIndex];
        if (character == '<')
        {
            if (urlSegmentIndex != std::string_view::npos)
            {
                return 0;
            }
            urlSegmentIndex = urlIndex;
        }
        if (character == '>')
        {
            if (urlSegmentIndex == std::string_view::npos)
            {
                return 0;
            }
            std::string_view tag = url.substr(urlSegmentIndex,
                                              urlIndex + 1 - urlSegmentIndex);

            // Note, this is a really lame way to do std::pow(6, paramIndex)
            // std::pow doesn't work in constexpr in clang.
            // Ideally in the future we'd move this to use a power of 2 packing
            // (probably 8 instead of 6) so that these just become bit shifts
            uint64_t insertIndex = 1;
            for (size_t unused = 0; unused < paramIndex; unused++)
            {
                insertIndex *= 3;
            }

            if (tag == "<str>" || tag == "<string>")
            {
                tagValue += insertIndex * toUnderlying(TypeCode::String);
            }
            if (tag == "<path>")
            {
                tagValue += insertIndex * toUnderlying(TypeCode::Path);
            }
            paramIndex++;
            urlSegmentIndex = std::string_view::npos;
        }
    }
    if (urlSegmentIndex != std::string_view::npos)
    {
        return 0;
    }
    return tagValue;
}

constexpr size_t numArgsFromTag(int tag)
{
    size_t ret = 0;
    while (tag > 0)
    {
        // Move to the next tag by removing the bottom bits from the number
        tag /= toUnderlying(TypeCode::Max);
        ret++;
    }
    return ret;
};

inline std::string base64encode(std::string_view data)
{
    const std::array<char, 64> key = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    size_t size = data.size();
    std::string ret;
    ret.resize((size + 2) / 3 * 4);
    auto it = ret.begin();

    size_t i = 0;
    while (i < size)
    {
        size_t keyIndex = 0;

        keyIndex = static_cast<size_t>(data[i] & 0xFC) >> 2;
        *it++ = key[keyIndex];

        if (i + 1 < size)
        {
            keyIndex = static_cast<size_t>(data[i] & 0x03) << 4;
            keyIndex += static_cast<size_t>(data[i + 1] & 0xF0) >> 4;
            *it++ = key[keyIndex];

            if (i + 2 < size)
            {
                keyIndex = static_cast<size_t>(data[i + 1] & 0x0F) << 2;
                keyIndex += static_cast<size_t>(data[i + 2] & 0xC0) >> 6;
                *it++ = key[keyIndex];

                keyIndex = static_cast<size_t>(data[i + 2] & 0x3F);
                *it++ = key[keyIndex];
            }
            else
            {
                keyIndex = static_cast<size_t>(data[i + 1] & 0x0F) << 2;
                *it++ = key[keyIndex];
                *it++ = '=';
            }
        }
        else
        {
            keyIndex = static_cast<size_t>(data[i] & 0x03) << 4;
            *it++ = key[keyIndex];
            *it++ = '=';
            *it++ = '=';
        }

        i += 3;
    }

    return ret;
}

// TODO this is temporary and should be deleted once base64 is refactored out of
// crow
inline bool base64Decode(std::string_view input, std::string& output)
{
    static const char nop = static_cast<char>(-1);
    // See note on encoding_data[] in above function
    static const std::array<char, 256> decodingData = {
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, 62,  nop, nop, nop, 63,  52,  53,  54,  55,  56,  57,  58,  59,
        60,  61,  nop, nop, nop, nop, nop, nop, nop, 0,   1,   2,   3,   4,
        5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,
        19,  20,  21,  22,  23,  24,  25,  nop, nop, nop, nop, nop, nop, 26,
        27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
        41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
        nop, nop, nop, nop};

    size_t inputLength = input.size();

    // allocate space for output string
    output.clear();
    output.reserve(((inputLength + 2) / 3) * 4);

    auto getCodeValue = [](char c) {
        auto code = static_cast<unsigned char>(c);
        // Ensure we cannot index outside the bounds of the decoding array
        static_assert(std::numeric_limits<decltype(code)>::max() <
                      decodingData.size());
        return decodingData[code];
    };

    // for each 4-bytes sequence from the input, extract 4 6-bits sequences by
    // dropping first two bits
    // and regenerate into 3 8-bits sequences

    for (size_t i = 0; i < inputLength; i++)
    {
        char base64code0 = 0;
        char base64code1 = 0;
        char base64code2 = 0; // initialized to 0 to suppress warnings

        base64code0 = getCodeValue(input[i]);
        if (base64code0 == nop)
        { // non base64 character
            return false;
        }
        if (!(++i < inputLength))
        { // we need at least two input bytes for first
          // byte output
            return false;
        }
        base64code1 = getCodeValue(input[i]);
        if (base64code1 == nop)
        { // non base64 character
            return false;
        }
        output +=
            static_cast<char>((base64code0 << 2) | ((base64code1 >> 4) & 0x3));

        if (++i < inputLength)
        {
            char c = input[i];
            if (c == '=')
            { // padding , end of input
                return (base64code1 & 0x0f) == 0;
            }
            base64code2 = getCodeValue(input[i]);
            if (base64code2 == nop)
            { // non base64 character
                return false;
            }
            output += static_cast<char>(((base64code1 << 4) & 0xf0) |
                                        ((base64code2 >> 2) & 0x0f));
        }

        if (++i < inputLength)
        {
            char c = input[i];
            if (c == '=')
            { // padding , end of input
                return (base64code2 & 0x03) == 0;
            }
            char base64code3 = getCodeValue(input[i]);
            if (base64code3 == nop)
            { // non base64 character
                return false;
            }
            output +=
                static_cast<char>((((base64code2 << 6) & 0xc0) | base64code3));
        }
    }

    return true;
}

inline bool constantTimeStringCompare(std::string_view a, std::string_view b)
{
    // Important note, this function is ONLY constant time if the two input
    // sizes are the same
    if (a.size() != b.size())
    {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

struct ConstantTimeCompare
{
    bool operator()(std::string_view a, std::string_view b) const
    {
        return constantTimeStringCompare(a, b);
    }
};

namespace details
{
inline boost::urls::url
    appendUrlPieces(boost::urls::url& url,
                    const std::initializer_list<std::string_view> args)
{
    for (std::string_view arg : args)
    {
        url.segments().push_back(arg);
    }
    return url;
}

} // namespace details

class OrMorePaths
{};

template <typename... AV>
inline void appendUrlPieces(boost::urls::url& url, const AV... args)
{
    details::appendUrlPieces(url, {args...});
}

namespace details
{

// std::reference_wrapper<std::string> - extracts segment to variable
//                    std::string_view - checks if segment is equal to variable
using UrlSegment = std::variant<std::reference_wrapper<std::string>,
                                std::string_view, OrMorePaths>;

enum class UrlParseResult
{
    Continue,
    Fail,
    Done,
};

class UrlSegmentMatcherVisitor
{
  public:
    UrlParseResult operator()(std::string& output)
    {
        output = segment;
        return UrlParseResult::Continue;
    }

    UrlParseResult operator()(std::string_view expected)
    {
        if (segment == expected)
        {
            return UrlParseResult::Continue;
        }
        return UrlParseResult::Fail;
    }

    UrlParseResult operator()(OrMorePaths /*unused*/)
    {
        return UrlParseResult::Done;
    }

    explicit UrlSegmentMatcherVisitor(std::string_view segmentIn) :
        segment(segmentIn)
    {}

  private:
    std::string_view segment;
};

inline bool readUrlSegments(boost::urls::url_view url,
                            std::initializer_list<UrlSegment>&& segments)
{
    boost::urls::segments_view urlSegments = url.segments();

    if (!urlSegments.is_absolute())
    {
        return false;
    }

    boost::urls::segments_view::iterator it = urlSegments.begin();
    boost::urls::segments_view::iterator end = urlSegments.end();

    for (const auto& segment : segments)
    {
        if (it == end)
        {
            // If the request ends with an "any" path, this was successful
            return std::holds_alternative<OrMorePaths>(segment);
        }
        UrlParseResult res = std::visit(UrlSegmentMatcherVisitor(*it), segment);
        if (res == UrlParseResult::Done)
        {
            return true;
        }
        if (res == UrlParseResult::Fail)
        {
            return false;
        }
        it++;
    }

    // There will be an empty segment at the end if the URI ends with a "/"
    // e.g. /redfish/v1/Chassis/
    if ((it != end) && urlSegments.back().empty())
    {
        it++;
    }
    return it == end;
}

} // namespace details

template <typename... Args>
inline bool readUrlSegments(boost::urls::url_view url, Args&&... args)
{
    return details::readUrlSegments(url, {std::forward<Args>(args)...});
}

inline boost::urls::url replaceUrlSegment(boost::urls::url_view urlView,
                                          const uint replaceLoc,
                                          std::string_view newSegment)
{
    boost::urls::segments_view urlSegments = urlView.segments();
    boost::urls::url url("/");

    if (!urlSegments.is_absolute())
    {
        return url;
    }

    boost::urls::segments_view::iterator it = urlSegments.begin();
    boost::urls::segments_view::iterator end = urlSegments.end();

    for (uint idx = 0; it != end; it++, idx++)
    {
        if (idx == replaceLoc)
        {
            url.segments().push_back(newSegment);
        }
        else
        {
            url.segments().push_back(*it);
        }
    }

    return url;
}

inline std::string setProtocolDefaults(boost::urls::url_view urlView)
{
    if (urlView.scheme() == "https")
    {
        return "https";
    }
    if (urlView.scheme() == "http")
    {
        if (bmcwebInsecureEnableHttpPushStyleEventing)
        {
            return "http";
        }
        return "";
    }
    if (urlView.scheme() == "snmp")
    {
        return "snmp";
    }
    return "";
}

inline uint16_t setPortDefaults(boost::urls::url_view url)
{
    uint16_t port = url.port_number();
    if (port != 0)
    {
        // user picked a port already.
        return port;
    }

    // If the user hasn't explicitly stated a port, pick one explicitly for them
    // based on the protocol defaults
    if (url.scheme() == "http")
    {
        return 80;
    }
    if (url.scheme() == "https")
    {
        return 443;
    }
    if (url.scheme() == "snmp")
    {
        return 162;
    }
    return 0;
}

inline bool validateAndSplitUrl(std::string_view destUrl, std::string& urlProto,
                                std::string& host, uint16_t& port,
                                std::string& path)
{
    boost::urls::result<boost::urls::url_view> url =
        boost::urls::parse_uri(destUrl);
    if (!url)
    {
        return false;
    }
    urlProto = setProtocolDefaults(url.value());
    if (urlProto.empty())
    {
        return false;
    }

    port = setPortDefaults(url.value());

    host = url->encoded_host_address();

    path = url->encoded_path();
    if (path.empty())
    {
        path = "/";
    }
    if (url->has_fragment())
    {
        path += '#';
        path += url->encoded_fragment();
    }

    if (url->has_query())
    {
        path += '?';
        path += url->encoded_query();
    }

    return true;
}

} // namespace utility
} // namespace crow

namespace nlohmann
{
template <>
struct adl_serializer<boost::urls::url>
{
    // nlohmann requires a specific casing to look these up in adl
    // NOLINTNEXTLINE(readability-identifier-naming)
    static void to_json(json& j, const boost::urls::url& url)
    {
        j = url.buffer();
    }
};

template <>
struct adl_serializer<boost::urls::url_view>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static void to_json(json& j, boost::urls::url_view url)
    {
        j = url.buffer();
    }
};
} // namespace nlohmann
