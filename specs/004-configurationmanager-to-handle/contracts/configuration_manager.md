# Contract: ConfigurationManager API (Header-Level)

## Initialization
- `static void loadOrDefault()`
- `static bool load()`
- `static bool save()`
- `static void onChanged(std::function<void()>)`

## Accessors (read)
- `bool getBool(const std::string& key, bool defaultValue)`
- `int64_t getInt(const std::string& key, int64_t defaultValue)`
- `double getDouble(const std::string& key, double defaultValue)`
- `std::string getString(const std::string& key, const std::string& defaultValue)`
- `std::vector<std::string> getStringList(const std::string& key, const std::vector<std::string>& defaultValue)`

## Mutators (write)
- `void set(const std::string& key, bool value)`
- `void set(const std::string& key, int64_t value)`
- `void set(const std::string& key, double value)`
- `void set(const std::string& key, const std::string& value)`
- `void set(const std::string& key, const std::vector<std::string>& value)`

## Behavior
- Thread-safe reads; serialized writes
- Atomic saves
- Env overrides applied at load time (`GB2D_` prefix)
- JSON file per platform path rules
