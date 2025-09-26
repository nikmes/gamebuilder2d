#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "ui/Windows/ConsoleLogWindow.h"
#include "ui/Windows/CodeEditorWindow.h"
#include "ui/Windows/FilePreviewWindow.h"

using nlohmann::json;
using namespace gb2d;

TEST_CASE("ConsoleLogWindow JSON round-trip", "[windows][json]") {
    ConsoleLogWindow a;
    // mutate some state
    json j1; a.serialize(j1);
    REQUIRE(j1.contains("title"));
    j1["title"] = std::string("Console X");
    j1["autoscroll"] = false;
    j1["max_lines"] = 1234;
    j1["buffer_cap"] = 2345;
    j1["level_mask"] = 0x15u;
    j1["text_filter"] = std::string("warn");

    ConsoleLogWindow b;
    b.deserialize(j1);
    json j2; b.serialize(j2);

    REQUIRE(j2["title"] == j1["title"]);
    REQUIRE(j2["autoscroll"] == j1["autoscroll"]);
    REQUIRE(j2["max_lines"] == j1["max_lines"]);
    // buffer_cap will be clamped to >= 1000 in deserialize
    REQUIRE((int)j2["buffer_cap"] >= 1000);
    REQUIRE(j2["level_mask"] == j1["level_mask"]);
    REQUIRE(j2["text_filter"] == j1["text_filter"]);
}

TEST_CASE("CodeEditorWindow JSON round-trip", "[windows][json]") {
    CodeEditorWindow a;
    // Simulate two tabs: one untitled, one with a bogus path (will still serialize path/title)
    a.newUntitled();
    a.openFile("tests/does_not_exist.txt");
    json j1; a.serialize(j1);
    REQUIRE(j1.contains("tabs"));
    REQUIRE(j1["tabs"].is_array());

    CodeEditorWindow b;
    b.deserialize(j1);
    json j2; b.serialize(j2);

    // We don't guarantee editor text equality, but we do expect tab metadata to match
    REQUIRE(j2["title"] == j1["title"]);
    REQUIRE(j2["current"].is_number_integer());
    REQUIRE(j2["tabs"].size() == j1["tabs"].size());
    for (size_t i = 0; i < j1["tabs"].size(); ++i) {
        auto &t1 = j1["tabs"][i];
        auto &t2 = j2["tabs"][i];
        REQUIRE(t2["title"] == t1["title"]);
        REQUIRE(t2["path"] == t1["path"]);
        REQUIRE(t2["dirty"].is_boolean());
    }
}

TEST_CASE("FilePreviewWindow JSON round-trip", "[windows][json]") {
    FilePreviewWindow a;
    json j1; a.serialize(j1);
    // Set a fake path; deserialize will try to open, but should not crash; on failure, it keeps state reasonable
    j1["title"] = std::string("Preview X");
    j1["path"] = std::string("tests/nope.png");

    FilePreviewWindow b;
    b.deserialize(j1);
    json j2; b.serialize(j2);

    REQUIRE(j2["title"].is_string());
    REQUIRE(j2["path"].is_string());
}
