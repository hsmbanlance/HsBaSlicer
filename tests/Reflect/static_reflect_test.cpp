#define BOOST_TEST_MODULE anyvisit_test
#include <boost/test/included/unit_test.hpp>

#include <base/static_reflect.hpp>
#include <base/any_visit.hpp>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

class Player
{
private:
	int health;
	float speed;
public:
	Player():health{100},speed{0.1f}
	{}
	void TakeDamage(int damage)
	{
		health -= damage;
	}
	int Heal(int amount)
	{
		health += amount;
		return health;
	}
	using FieldList = std::tuple <
		HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, int, "health"_ts, &Player::health>,
		HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, float, "speed"_ts, &Player::speed>>;
	using MethodList = std::tuple <
		HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, void(int), "TakeDamage"_ts, &Player::TakeDamage>,
		HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, int(int), "Heal"_ts, &Player::Heal>>;
	constexpr static auto ClassName = "Player"_ts;
};

BOOST_AUTO_TEST_SUITE(static_reflect)

BOOST_AUTO_TEST_CASE(test_static_reflect)
{
	BOOST_TEST_MESSAGE("Running static_reflect test");
	using PlayerReflect = HsBa::Slicer::Utils::StaticReflect::Reflector<Player>;
	BOOST_REQUIRE_MESSAGE(PlayerReflect::ClassName() == "Player", "Class name mismatch");
	BOOST_REQUIRE_MESSAGE(PlayerReflect::FieldCount() == 2, "Field count mismatch");
	BOOST_REQUIRE_MESSAGE(PlayerReflect::MethodCount() == 2, "Method count mismatch");
	Player player;
	// Test field access
	auto& healthField = PlayerReflect::GetField<0>(player);
	auto& speedField = PlayerReflect::GetField<1>(player);
	BOOST_REQUIRE_MESSAGE(healthField == 100, "Initial health mismatch");
	BOOST_REQUIRE_MESSAGE(speedField == 0.1f, "Initial speed mismatch");
	// Test method invocation
	PlayerReflect::InvokeMemberFunction<"TakeDamage"_ts>(player, 30);
	BOOST_REQUIRE_MESSAGE(healthField == 70, "Health after damage mismatch");
	PlayerReflect::InvokeMemberFunction<"Heal"_ts>(player, 20);
	BOOST_REQUIRE_MESSAGE(healthField == 90, "Health after healing mismatch");
}

BOOST_AUTO_TEST_SUITE_END()