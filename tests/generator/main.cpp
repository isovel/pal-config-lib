// A mod's generator tool, in the shape the README recommends.

#include <PalCfg/Generate.hpp>
#include <PalCfg/Schema.hpp>

namespace
{
    struct Settings
    {
        bool Enabled = true;
        float Rate = 0.08f;
    };

    inline constexpr auto kSchema = PalCfg::Schema<Settings>("Example")
                                        .Field("enabled", &Settings::Enabled)
                                        .Help("Whether the mod does anything at all.")
                                        .Field("rate", &Settings::Rate)
                                        .Help("Weight per second while rising.");

    inline constexpr auto kFields = kSchema.Flatten();
} // namespace

int main(int argc, char** argv)
{
    return PalCfg::GenerateMain(argc, argv, kFields, Settings{});
}
