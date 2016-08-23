#include <cpp-pcp-client/validator/validator.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE CPP_PCP_CLIENT_LOGGING_PREFIX".validator"
#include <leatherman/logging/logging.hpp>

#include <leatherman/locale/locale.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <cpp-pcp-client/valijson/rapidjson_adapter.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>
#pragma GCC diagnostic pop

namespace PCPClient {

namespace lth_jc   = leatherman::json_container;
namespace lth_loc  = leatherman::locale;
namespace lth_util = leatherman::util;

///
/// Auxiliary functions
///

std::string getValidationError(valijson::ValidationResults& validation_results) {
    std::string err_msg {};
    valijson::ValidationResults::Error error;
    unsigned int err_idx { 0 };
    static std::string err_label { lth_loc::translate("ERROR") };

    while (validation_results.popError(error)) {
        if (!err_msg.empty()) {
            err_msg += "  - ";
        }
        err_idx++;
        err_msg += err_label + std::to_string(err_idx) + ":";
        for (const auto& context_element : error.context) {
            err_msg += " " + context_element;
        }
    }

    return  err_msg;
}

bool validateJsonContainer(const lth_jc::JsonContainer& data, const Schema& schema) {
    valijson::Validator validator { schema.getRaw() };
    valijson::adapters::RapidJsonAdapter adapted_document { data.getRaw() };
    valijson::ValidationResults validation_results;

    auto success = validator.validate(adapted_document, &validation_results);

    if (!success) {
        auto err_msg = getValidationError(validation_results);
        LOG_DEBUG("Schema validation failure: {1}", err_msg);
    }

    return success;
}

///
/// Public API
///

Validator::Validator()
        : schema_map_ {},
          lookup_mutex_ {} {
}

Validator::Validator(Validator&& other_validator)
        : schema_map_ { other_validator.schema_map_ },
          lookup_mutex_ {} {
}

void Validator::registerSchema(const Schema& schema) {
    lth_util::lock_guard<lth_util::mutex> lock(lookup_mutex_);
    auto schema_name = schema.getName();
    if (includesSchema(schema_name)) {
        throw schema_redefinition_error {
            lth_loc::format("schema '{1}' already defined", schema_name) };
    }

    auto p = std::pair<std::string, Schema>(schema_name, schema);
    schema_map_.insert(p);
}

void Validator::validate(const lth_jc::JsonContainer& data,
                         std::string schema_name) const {
    lth_util::unique_lock<lth_util::mutex> lock(lookup_mutex_);
    if (!includesSchema(schema_name)) {
        throw schema_not_found_error {
            lth_loc::format("'{1}' is not a registered schema", schema_name) };
    }
    lock.unlock();

    // we can freely unlock. When a schema has been set it cannot be modified

    if (!validateJsonContainer(data, schema_map_.at(schema_name))) {
        throw validation_error {
            lth_loc::format("does not match schema: '{1}'", schema_name) };
    }
}

bool Validator::includesSchema(std::string schema_name) const {
    return schema_map_.find(schema_name) != schema_map_.end();
}

ContentType Validator::getSchemaContentType(std::string schema_name) const {
    lth_util::unique_lock<lth_util::mutex> lock(lookup_mutex_);
    if (!includesSchema(schema_name)) {
        throw schema_not_found_error {
            lth_loc::format("'{1}' is not a registered schema", schema_name) };
    }
    lock.unlock();

    return schema_map_.at(schema_name).getContentType();
}

}  // namespace PCPClient
