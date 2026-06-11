#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <dlfcn.h>
#include <memory>
#include <stdexcept>
#include <string>

#include "vulcan.h"

namespace py = pybind11;

namespace {

class PolicyPlugin {
public:
    explicit PolicyPlugin(const std::string& path) : path_(path) {
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle_) {
            throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
        }
    }
    ~PolicyPlugin() { if (handle_) dlclose(handle_); }
    PolicyPlugin(const PolicyPlugin&) = delete;
    PolicyPlugin& operator=(const PolicyPlugin&) = delete;

    void configure_rank(vulcan::feature_registry& reg, vulcan::rank_config& cfg) const {
        using fn_t = void(*)(vulcan::feature_registry&, vulcan::rank_config&);
        dlerror();
        auto fn = reinterpret_cast<fn_t>(dlsym(handle_, "vulcan_configure_rank"));
        const char* err = dlerror();
        if (err || !fn) {
            throw std::runtime_error(
                "Symbol 'vulcan_configure_rank' not found in " + path_ +
                (err ? std::string(": ") + err : std::string{}));
        }
        fn(reg, cfg);
    }

    void configure_value(vulcan::feature_registry& reg, vulcan::value_config& cfg) const {
        using fn_t = void(*)(vulcan::feature_registry&, vulcan::value_config&);
        dlerror();
        auto fn = reinterpret_cast<fn_t>(dlsym(handle_, "vulcan_configure_value"));
        const char* err = dlerror();
        if (err || !fn) {
            throw std::runtime_error(
                "Symbol 'vulcan_configure_value' not found in " + path_ +
                (err ? std::string(": ") + err : std::string{}));
        }
        fn(reg, cfg);
    }

private:
    std::string path_;
    void* handle_ = nullptr;
};

} // namespace

PYBIND11_MODULE(_vulcan, m) {
    m.doc() = "Python bindings for libvulcan";

    // ---- Typed feature handles -------------------------------------------------
    py::class_<vulcan::feature_handle<double>>(m, "HandleF64")
        .def_readonly("id", &vulcan::feature_handle<double>::id)
        .def_readonly("name", &vulcan::feature_handle<double>::name)
        .def("__repr__", [](const vulcan::feature_handle<double>& h) {
            return "<HandleF64 id=" + std::to_string(h.id) + " name='" + h.name + "'>";
        });

    py::class_<vulcan::feature_handle<int64_t>>(m, "HandleI64")
        .def_readonly("id", &vulcan::feature_handle<int64_t>::id)
        .def_readonly("name", &vulcan::feature_handle<int64_t>::name)
        .def("__repr__", [](const vulcan::feature_handle<int64_t>& h) {
            return "<HandleI64 id=" + std::to_string(h.id) + " name='" + h.name + "'>";
        });

    // ---- Registry proxies ------------------------------------------------------
    py::class_<vulcan::feature_registry::GlobalFeatures>(m, "GlobalFeatures")
        .def("declare_f64", &vulcan::feature_registry::GlobalFeatures::declare_f64,
             py::arg("name"), py::arg("description"))
        .def("declare_i64", &vulcan::feature_registry::GlobalFeatures::declare_i64,
             py::arg("name"), py::arg("description"))
        .def("lookup_f64", &vulcan::feature_registry::GlobalFeatures::lookup_f64, py::arg("name"))
        .def("lookup_i64", &vulcan::feature_registry::GlobalFeatures::lookup_i64, py::arg("name"));

    py::class_<vulcan::feature_registry::ObjectFeatures>(m, "ObjectFeatures")
        .def("declare_f64", &vulcan::feature_registry::ObjectFeatures::declare_f64,
             py::arg("name"), py::arg("description"))
        .def("declare_i64", &vulcan::feature_registry::ObjectFeatures::declare_i64,
             py::arg("name"), py::arg("description"))
        .def("lookup_f64", &vulcan::feature_registry::ObjectFeatures::lookup_f64, py::arg("name"))
        .def("lookup_i64", &vulcan::feature_registry::ObjectFeatures::lookup_i64, py::arg("name"));

    py::class_<vulcan::feature_registry>(m, "FeatureRegistry")
        .def(py::init<>())
        .def_property_readonly("global_features",
            [](vulcan::feature_registry& r) -> vulcan::feature_registry::GlobalFeatures& { return r.global; },
            py::return_value_policy::reference_internal)
        .def_property_readonly("object_features",
            [](vulcan::feature_registry& r) -> vulcan::feature_registry::ObjectFeatures& { return r.object; },
            py::return_value_policy::reference_internal);

    // ---- Configs ---------------------------------------------------------------
    py::class_<vulcan::policy_config>(m, "PolicyConfig")
        .def("set_information", &vulcan::policy_config::set_information, py::arg("info"))
        .def("get_information", &vulcan::policy_config::get_information,
             py::return_value_policy::reference_internal);

    py::class_<vulcan::rank_config, vulcan::policy_config>(m, "RankConfig")
        .def(py::init<>());

    py::class_<vulcan::value_config, vulcan::policy_config>(m, "ValueConfig")
        .def(py::init<>());

    // ---- Feature store ---------------------------------------------------------
    py::class_<vulcan::feature_store>(m, "FeatureStore")
        .def("update",
             py::overload_cast<vulcan::feature_handle<double>, double>(&vulcan::feature_store::update),
             py::arg("handle"), py::arg("value"))
        .def("update",
             py::overload_cast<vulcan::feature_handle<int64_t>, int64_t>(&vulcan::feature_store::update),
             py::arg("handle"), py::arg("value"))
        .def("update",
             py::overload_cast<vulcan::feature_handle<double>, int64_t, double>(&vulcan::feature_store::update),
             py::arg("handle"), py::arg("obj_id"), py::arg("value"))
        .def("update",
             py::overload_cast<vulcan::feature_handle<int64_t>, int64_t, int64_t>(&vulcan::feature_store::update),
             py::arg("handle"), py::arg("obj_id"), py::arg("value"));

    // ---- Policies --------------------------------------------------------------
    py::class_<vulcan::rank_policy>(m, "RankPolicy")
        .def_property_readonly("feature_store",
            py::overload_cast<>(&vulcan::rank_policy::get_feature_store),
            py::return_value_policy::reference_internal)
        .def("add_object", &vulcan::rank_policy::add_object, py::arg("obj_id"))
        .def("remove_object", &vulcan::rank_policy::remove_object, py::arg("obj_id"))
        .def("decide", &vulcan::rank_policy::decide)
        .def("get_prompt", &vulcan::rank_policy::get_prompt);

    py::class_<vulcan::value_policy>(m, "ValuePolicy")
        .def_property_readonly("feature_store",
            py::overload_cast<>(&vulcan::value_policy::get_feature_store),
            py::return_value_policy::reference_internal)
        .def("decide", &vulcan::value_policy::decide)
        .def("get_prompt", &vulcan::value_policy::get_prompt);

    m.def("instantiate_rank_policy",
        [](const vulcan::feature_registry& reg, const vulcan::rank_config& cfg) {
            return std::make_unique<vulcan::rank_policy>(reg, cfg);
        }, py::arg("registry"), py::arg("config"));

    m.def("instantiate_value_policy",
        [](const vulcan::feature_registry& reg, const vulcan::value_config& cfg) {
            return std::make_unique<vulcan::value_policy>(reg, cfg);
        }, py::arg("registry"), py::arg("config"));

    // ---- Policy plugin loader --------------------------------------------------
    py::class_<PolicyPlugin>(m, "PolicyPlugin")
        .def("configure_rank", &PolicyPlugin::configure_rank,
             py::arg("registry"), py::arg("config"))
        .def("configure_value", &PolicyPlugin::configure_value,
             py::arg("registry"), py::arg("config"));

    m.def("load_policy", [](const std::string& path) {
        return std::make_unique<PolicyPlugin>(path);
    }, py::arg("path"),
       "Load a compiled .so containing an EVOLVE-block policy. "
       "The .so must export extern \"C\" vulcan_configure_rank or vulcan_configure_value.");
}
