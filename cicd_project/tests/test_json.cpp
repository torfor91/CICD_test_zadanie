#include "../src/test_framework.hpp"
#include "../src/json.hpp"

TEST(json_parse_object) {
    auto v = json::parse(R"({"a": 1, "b": "hi", "c": true})");
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ((int)v["a"].num, 1);
    ASSERT_EQ(v["b"].str, std::string("hi"));
    ASSERT_TRUE(v["c"].b);
}

TEST(json_parse_array) {
    auto v = json::parse(R"([1, 2, 3])");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ((int)v[0].num, 1);
    ASSERT_EQ((int)v[2].num, 3);
}

TEST(json_pipeline_example) {
    auto v = json::parse(R"({"pipeline":[{"stage":"build","source":"./src"}]})");
    ASSERT_TRUE(v["pipeline"].is_array());
    ASSERT_EQ(v["pipeline"][0]["stage"].str, std::string("build"));
}

TEST(json_invalid_throws) {
    bool thrown = false;
    try { json::parse("{invalid}"); }
    catch (...) { thrown = true; }
    ASSERT_TRUE(thrown);
}