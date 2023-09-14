/* SPDX-License-Identifier: BSL-1.0 OR BSD-3-Clause */

#ifndef MPT_JSON_JSON_HPP
#define MPT_JSON_JSON_HPP

#include "mpt/base/detect.hpp"
#include "mpt/detect/nlohmann_json.hpp"

#if MPT_DETECTED_NLOHMANN_JSON
#include "mpt/string/types.hpp"
#include "mpt/string_transcode/transcode.hpp"
#endif // MPT_DETECTED_NLOHMANN_JSON

#if MPT_DETECTED_NLOHMANN_JSON
#include <optional>
#endif // MPT_DETECTED_NLOHMANN_JSON

#if MPT_DETECTED_NLOHMANN_JSON
#pragma warning(push)
#pragma warning(disable:28020)
#include <nlohmann/json.hpp>
#pragma warning(pop)
#endif // MPT_DETECTED_NLOHMANN_JSON



namespace nlohmann {
template <>
struct adl_serializer<mpt::ustring> {
	static void to_json(json & j, const mpt::ustring & val) {
		j = mpt::transcode<std::string>(mpt::common_encoding::utf8, val);
	}
	static void from_json(const json & j, mpt::ustring & val) {
		val = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, j.get<std::string>());
	}
};
template <typename Tvalue>
struct adl_serializer<std::map<mpt::ustring, Tvalue>> {
	static void to_json(json & j, const std::map<mpt::ustring, Tvalue> & val) {
		std::map<std::string, Tvalue> utf8map;
		for (const auto & value : val) {
			utf8map[mpt::transcode<std::string>(mpt::common_encoding::utf8, value.first)] = value.second;
		}
		j = std::move(utf8map);
	}
	static void from_json(const json & j, std::map<mpt::ustring, Tvalue> & val) {
		std::map<std::string, Tvalue> utf8map = j.get<std::map<std::string, Tvalue>>();
		std::map<mpt::ustring, Tvalue> result;
		for (const auto & value : utf8map) {
			result[mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, value.first)] = value.second;
		}
		val = std::move(result);
	}
};
template <typename Tvalue>
struct adl_serializer<std::optional<Tvalue>> {
	static void to_json(json & j, const std::optional<Tvalue> & val) {
		j = (val ? json{*val} : json{nullptr});
	}
	static void from_json(const json & j, std::optional<Tvalue> & val) {
		if (!j.is_null()) {
			val = j.get<Tvalue>();
		} else {
			val = std::nullopt;
		}
	}
};
} // namespace nlohmann



#endif // MPT_JSON_JSON_HPP
