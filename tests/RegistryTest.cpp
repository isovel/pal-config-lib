#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

#include <PalCfgRegistry.h>

namespace
{
    // A fake mod whose strings come from its own counter-tracked allocator, so
    // a registry that frees them with the wrong function is caught.
    struct FakeMod
    {
        std::string document = R"({"count": 1})";
        std::string lastSet;
        int allocated = 0;
        int freed = 0;

        char* Dup(const std::string& text)
        {
            ++allocated;
            char* out = new char[text.size() + 1];
            std::memcpy(out, text.c_str(), text.size() + 1);
            return out;
        }

        static char* Get(void* user)
        {
            auto& self = *static_cast<FakeMod*>(user);
            return self.Dup(self.document);
        }

        static char* Set(void* user, const char* json)
        {
            auto& self = *static_cast<FakeMod*>(user);
            self.lastSet = json;
            return self.Dup("[]");
        }

        static char* GetNull(void*) { return nullptr; }

        static void Free(char* text)
        {
            // Test-global counter, since the callback has no user pointer.
            ++s_freed;
            delete[] text;
        }

        static inline int s_freed = 0;

        PalCfgModDesc Desc(const char* name)
        {
            return PalCfgModDesc{PALCFG_REGISTRY_ABI, name, R"({"name":"x","fields":[]})", this, &Get, &Set, &Free};
        }
    };

    struct Registration
    {
        PalCfgMod* mod;
        ~Registration() { PalCfgRegistry_Unregister(mod); }
    };
}

TEST_CASE("the registry reports its ABI")
{
    CHECK(PalCfgRegistry_Abi() == PALCFG_REGISTRY_ABI);
}

TEST_CASE("register then unregister leaves the table empty")
{
    FakeMod a;
    const auto desc = a.Desc("A");

    PalCfgMod* mod = PalCfgRegistry_Register(&desc);
    REQUIRE(mod != nullptr);
    CHECK(PalCfgRegistry_Count() == 1);
    CHECK(std::string{PalCfgRegistry_Name(0)} == "A");
    CHECK(std::string{PalCfgRegistry_Schema(0)} == R"({"name":"x","fields":[]})");

    PalCfgRegistry_Unregister(mod);
    CHECK(PalCfgRegistry_Count() == 0);
}

TEST_CASE("register refuses a bad ABI, a duplicate name and missing callbacks")
{
    FakeMod a;
    auto desc = a.Desc("A");
    Registration first{PalCfgRegistry_Register(&desc)};
    REQUIRE(first.mod != nullptr);

    auto dup = a.Desc("A");
    CHECK(PalCfgRegistry_Register(&dup) == nullptr);

    auto wrongAbi = a.Desc("B");
    wrongAbi.abi = 99;
    CHECK(PalCfgRegistry_Register(&wrongAbi) == nullptr);

    auto noName = a.Desc(nullptr);
    CHECK(PalCfgRegistry_Register(&noName) == nullptr);

    auto noGet = a.Desc("C");
    noGet.getDocument = nullptr;
    CHECK(PalCfgRegistry_Register(&noGet) == nullptr);

    auto noSet = a.Desc("D");
    noSet.setDocument = nullptr;
    CHECK(PalCfgRegistry_Register(&noSet) == nullptr);

    auto noFree = a.Desc("E");
    noFree.freeText = nullptr;
    CHECK(PalCfgRegistry_Register(&noFree) == nullptr);

    CHECK(PalCfgRegistry_Register(nullptr) == nullptr);
    CHECK(PalCfgRegistry_Count() == 1);
}

TEST_CASE("documents are copied into registry memory and mod memory is freed by the mod")
{
    FakeMod a;
    FakeMod::s_freed = 0;
    auto desc = a.Desc("A");
    Registration reg{PalCfgRegistry_Register(&desc)};

    char* doc = PalCfgRegistry_GetDocument(0);
    REQUIRE(doc != nullptr);
    CHECK(std::string{doc} == R"({"count": 1})");
    CHECK(a.allocated == 1);
    CHECK(FakeMod::s_freed == 1);
    PalCfgRegistry_Free(doc);

    char* result = PalCfgRegistry_SetDocument(0, R"({"count": 2})");
    REQUIRE(result != nullptr);
    CHECK(std::string{result} == "[]");
    CHECK(a.lastSet == R"({"count": 2})");
    CHECK(FakeMod::s_freed == 2);
    PalCfgRegistry_Free(result);
}

TEST_CASE("out-of-range indices return null")
{
    CHECK(PalCfgRegistry_Name(0) == nullptr);
    CHECK(PalCfgRegistry_Schema(0) == nullptr);
    CHECK(PalCfgRegistry_GetDocument(0) == nullptr);
    CHECK(PalCfgRegistry_SetDocument(0, "{}") == nullptr);
    PalCfgRegistry_Free(nullptr);
    PalCfgRegistry_Unregister(nullptr);
}

TEST_CASE("a mod returning nothing surfaces as an Error diagnostic")
{
    FakeMod a;
    auto desc = a.Desc("A");
    desc.getDocument = &FakeMod::GetNull;
    Registration reg{PalCfgRegistry_Register(&desc)};

    char* doc = PalCfgRegistry_GetDocument(0);
    REQUIRE(doc != nullptr);
    CHECK(std::string{doc} == R"([{"severity":"Error","field":"","message":"mod returned nothing"}])");
    PalCfgRegistry_Free(doc);
}

TEST_CASE("two mods enumerate in registration order and unregistering one shifts indices")
{
    FakeMod a, b;
    auto da = a.Desc("A");
    auto db = b.Desc("B");
    PalCfgMod* ma = PalCfgRegistry_Register(&da);
    Registration rb{PalCfgRegistry_Register(&db)};

    CHECK(PalCfgRegistry_Count() == 2);
    CHECK(std::string{PalCfgRegistry_Name(0)} == "A");
    CHECK(std::string{PalCfgRegistry_Name(1)} == "B");

    PalCfgRegistry_Unregister(ma);
    CHECK(PalCfgRegistry_Count() == 1);
    CHECK(std::string{PalCfgRegistry_Name(0)} == "B");
}
