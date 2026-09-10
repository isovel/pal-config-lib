#error "A public PalCfg header included nlohmann/json.hpp. Keep it behind the IValueSource seam in src/core - consumers must not pay ~1MB of parser in their own translation units."
