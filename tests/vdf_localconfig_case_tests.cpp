// Regression test: localconfig.vdf section lookup must not depend on the
// exact casing Steam happens to write.
//
// Steam writes  UserLocalConfigStore > Software > valve > Steam > apps  with a
// lowercase "valve" parent on current builds. StatsStore::ReconcileLocalConfig
// builds that path with "Valve" and only retries the *leaf* key ("apps"/"Apps"),
// so with a byte-exact lookup the whole reconcile aborts with
// "apps section not found" and playtime is never imported from localconfig.vdf.
//
// Valve's KeyValues format treats key names case-insensitively, so the lookup
// itself is what has to be tolerant here.
#include "vdf.h"

#include <cstdio>
#include <string>
#include <string_view>

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

// Shape observed in Steam's userdata/<account>/config/localconfig.vdf.
static const char* kLowercaseValve = R"VDF("UserLocalConfigStore"
{
	"Software"
	{
		"valve"
		{
			"Steam"
			{
				"apps"
				{
					"294100"
					{
						"LastPlayed"		"1794179956"
						"Playtime"		"1029"
						"Playtime2wks"		"1029"
					}
				}
			}
		}
	}
}
)VDF";

// The capitalisation the code currently assumes.
static const char* kCapitalValve = R"VDF("UserLocalConfigStore"
{
	"Software"
	{
		"Valve"
		{
			"Steam"
			{
				"apps"
				{
					"294100"
					{
						"Playtime"		"1029"
					}
				}
			}
		}
	}
}
)VDF";

// Exactly the array ReconcileLocalConfig builds (stats_store.cpp).
static const char* kReconcilePath[] = {
    "UserLocalConfigStore", "Software", "Valve", "Steam", "apps"};

static void TestLowercaseValveParentIsFound() {
    size_t start = 0, end = 0;
    CHECK(VdfUtil::FindVdfSectionRange(std::string(kLowercaseValve), kReconcilePath, 5,
                                       start, end));
}

static void TestCapitalisedParentIsStillFound() {
    size_t start = 0, end = 0;
    CHECK(VdfUtil::FindVdfSectionRange(std::string(kCapitalValve), kReconcilePath, 5,
                                       start, end));
}

static void TestUppercaseLeafKeyIsFound() {
    const char* path[] = {"UserLocalConfigStore", "Software", "Valve", "Steam", "Apps"};
    size_t start = 0, end = 0;
    CHECK(VdfUtil::FindVdfSectionRange(std::string(kLowercaseValve), path, 5, start, end));
}

static void TestReconcileReadsPlaytimeFromLowercaseTree() {
    const char* appPath[] = {"UserLocalConfigStore", "Software", "Valve", "Steam", "apps",
                             "294100"};
    uint32_t playtime = 0;
    uint32_t lastPlayed = 0;
    VdfUtil::ForEachFieldInSection(kLowercaseValve, appPath, 6,
                                   [&](const VdfUtil::FieldInfo& fi) -> bool {
                                       if (fi.key == "Playtime")
                                           playtime = (uint32_t)std::stoul(std::string(fi.value));
                                       else if (fi.key == "LastPlayed")
                                           lastPlayed = (uint32_t)std::stoul(std::string(fi.value));
                                       return true;
                                   });
    CHECK(playtime == 1029);
    CHECK(lastPlayed == 1794179956u);
}

static void TestAppIdsEnumeratedFromLowercaseTree() {
    int children = 0;
    bool sawApp = false;
    VdfUtil::ForEachChildInSection(kLowercaseValve, kReconcilePath, 5,
                                   [&](std::string_view name) -> bool {
                                       ++children;
                                       if (name == "294100") sawApp = true;
                                       return true;
                                   });
    CHECK(children == 1);
    CHECK(sawApp);
}

// Guard against a fuzzy matcher: a key of equal length that differs must not match.
static void TestUnrelatedKeyDoesNotMatch() {
    const char* path[] = {"UserLocalConfigStore", "Software", "Velve", "Steam", "apps"};
    size_t start = 0, end = 0;
    CHECK(!VdfUtil::FindVdfSectionRange(std::string(kLowercaseValve), path, 5, start, end));
}

int main() {
    TestLowercaseValveParentIsFound();
    TestCapitalisedParentIsStillFound();
    TestUppercaseLeafKeyIsFound();
    TestReconcileReadsPlaytimeFromLowercaseTree();
    TestAppIdsEnumeratedFromLowercaseTree();
    TestUnrelatedKeyDoesNotMatch();

    if (g_failures == 0) {
        std::printf("vdf_localconfig_case_tests: all tests passed\n");
        return 0;
    }
    std::printf("vdf_localconfig_case_tests: %d failure(s)\n", g_failures);
    return 1;
}
