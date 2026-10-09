// BattleEffectTests — 回合制能力系统(阶段2)自动化测试
//
// 自包含:用 transient UObject 构造 UAttributeComponent + UCombatStatusComponent,
// 不依赖 World/Actor(属性数学与状态时效都是纯逻辑)。
// 跑:UnrealEditor-cmd <proj> -ExecCmds="Automation RunTests AiGame.Battle; Quit" -unattended -nullrhi

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Components/AttributeComponent.h"
#include "Components/CombatStatusComponent.h"
#include "BattleEffect.h"
#include "BattleDamageCalculator.h"
#include "SkillEffectResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatStatusEffectTest,
	"AiGame.Battle.CombatStatusEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatStatusEffectTest::RunTest(const FString& Parameters)
{
	UPackage* Pkg = GetTransientPackage();

	// —— 目标属性:HP100 / SPD50 ——
	UAttributeComponent* Attr = NewObject<UAttributeComponent>(Pkg);
	Attr->SetBaseValue(TEXT("HP"), 100.f);
	Attr->SetBaseValue(TEXT("MaxHP"), 100.f);
	Attr->SetBaseValue(TEXT("SPD"), 50.f);

	// —— 施法者:Spirit 0(简化,效果数值=配置值)——
	UAttributeComponent* Caster = NewObject<UAttributeComponent>(Pkg);
	Caster->SetBaseValue(TEXT("Spirit"), 0.f);

	UCombatStatusComponent* CS = NewObject<UCombatStatusComponent>(Pkg);
	CS->SetAttributeComponent(Attr);

	// ① 伤害:power 30 → HP 70
	{
		FEffectComponent E; E.Type = EEffectType::Damage; E.Power = 30;
		CS->ApplyEffect(E, Caster, TEXT("SK_fireball"));
		TestEqual(TEXT("伤害后 HP"), Attr->GetFinalValue(TEXT("HP")), 70.f);
	}

	// ② 减速:SPD flat -20,2 回合 → SPD 30
	{
		FEffectComponent E; E.Type = EEffectType::Buff; E.Attr = TEXT("SPD");
		E.bPercent = false; E.Value = -20.f; E.Duration = 2; E.MaxStacks = 1;
		CS->ApplyEffect(E, Caster, TEXT("SK_fireball"));
		TestEqual(TEXT("减速后 SPD"), Attr->GetFinalValue(TEXT("SPD")), 30.f);
	}

	// ③ 百分比 buff:ATK +50%,基础 0 → 仍 0(验证 percent 走 Multiply 不报错);改测 SPD percent
	{
		// SPD 当前 base50 已有 flat -20;再加 percent +20%(modifier 独立 source)
		FEffectComponent E; E.Type = EEffectType::Buff; E.Attr = TEXT("SPD");
		E.bPercent = true; E.Value = 20.f; E.Duration = 5; E.MaxStacks = 1;
		CS->ApplyEffect(E, Caster, TEXT("SK_haste"));
		// 终值 = (50 + (-20)) * (1 + 0.20) = 30 * 1.2 = 36
		TestEqual(TEXT("flat+percent 叠加"), Attr->GetFinalValue(TEXT("SPD")), 36.f);
		// 清掉这条 haste 以免干扰后续 SPD 断言
		Attr->RemoveModifiersBySource(TEXT("SK_haste"));
		TestEqual(TEXT("移除 haste 后 SPD"), Attr->GetFinalValue(TEXT("SPD")), 30.f);
	}

	// ④ 中毒:10/回合,3 回合,可叠 3 层
	{
		FEffectComponent E; E.Type = EEffectType::Dot; E.DamagePerTurn = 10; E.Duration = 3; E.MaxStacks = 3;
		CS->ApplyEffect(E, Caster, TEXT("SK_poison"));
	}

	// ⑤ 眩晕:1 回合
	{
		FEffectComponent E; E.Type = EEffectType::Control; E.Control = CombatTag::Stunned; E.Duration = 1;
		CS->ApplyEffect(E, Caster, TEXT("SK_bash"));
		TestTrue(TEXT("已眩晕"), CS->IsStunned());
		TestFalse(TEXT("眩晕不能行动"), CS->CanAct());
	}

	// —— 回合 tick 1:中毒扣10→HP60;眩晕1→0移除;减速2→1 ——
	{
		FCombatTurnTickResult R = CS->TickTurnStart();
		TestEqual(TEXT("tick1 中毒伤害"), R.PoisonDamage, 10);
		TestEqual(TEXT("tick1 后 HP"), Attr->GetFinalValue(TEXT("HP")), 60.f);
		TestFalse(TEXT("tick1 后眩晕已解"), CS->IsStunned());
		TestEqual(TEXT("tick1 减速仍在"), Attr->GetFinalValue(TEXT("SPD")), 30.f);
	}

	// —— 回合 tick 2:中毒→HP50;减速1→0移除 SPD恢复50 ——
	{
		FCombatTurnTickResult R = CS->TickTurnStart();
		TestEqual(TEXT("tick2 后 HP"), Attr->GetFinalValue(TEXT("HP")), 50.f);
		TestEqual(TEXT("tick2 减速到期 SPD恢复"), Attr->GetFinalValue(TEXT("SPD")), 50.f);
	}

	// —— 中毒叠层:再施一次同源中毒 → 2 层,刷新时效 ——
	{
		FEffectComponent E; E.Type = EEffectType::Dot; E.DamagePerTurn = 10; E.Duration = 3; E.MaxStacks = 3;
		CS->ApplyEffect(E, Caster, TEXT("SK_poison"));
		TestEqual(TEXT("中毒叠 2 层"), CS->GetDotStacks(), 2);
		FCombatTurnTickResult R = CS->TickTurnStart();
		TestEqual(TEXT("2 层中毒伤害=20"), R.PoisonDamage, 20);
		TestEqual(TEXT("叠层后 HP"), Attr->GetFinalValue(TEXT("HP")), 30.f);
	}

	// —— 治疗:+40 → HP70(不超 MaxHP100)——
	{
		FEffectComponent E; E.Type = EEffectType::Heal; E.Amount = 40;
		CS->ApplyEffect(E, Caster, TEXT("SK_heal"));
		TestEqual(TEXT("治疗后 HP"), Attr->GetFinalValue(TEXT("HP")), 70.f);
	}

	return true;
}

// ============================================================
// 技能执行器:从真实 SK_fireball.json 解析效果计划(阶段3)
// ============================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillResolverLoadPlanTest,
	"AiGame.Battle.SkillResolverLoadPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillResolverLoadPlanTest::RunTest(const FString& Parameters)
{
	FSkillEffectPlan Plan;
	const bool bOk = FSkillEffectResolver::LoadPlan(TEXT("Test/SK_fireball"), Plan);
	TestTrue(TEXT("加载 SK_fireball 计划"), bOk);
	if (!bOk) return false;

	// 数据页 battle 配置
	TestTrue(TEXT("有 battle 配置"), Plan.bHasBattle);
	TestEqual(TEXT("目标类型"), Plan.TargetType, FString(TEXT("single_enemy")));
	TestEqual(TEXT("MP消耗"), Plan.MpCost, 12);

	// 逻辑页时间线:1 判定 + 2 效果
	TestEqual(TEXT("判定数"), Plan.HitTests.Num(), 1);
	if (Plan.HitTests.Num() == 1)
	{
		TestEqual(TEXT("判定ID"), Plan.HitTests[0].HitId, FString(TEXT("H1")));
		TestEqual(TEXT("判定范围"), Plan.HitTests[0].Scope, FString(TEXT("single")));
		TestEqual(TEXT("判定阵营"), Plan.HitTests[0].Faction, FString(TEXT("enemy")));
	}
	TestEqual(TEXT("效果数"), Plan.Effects.Num(), 2);
	TestTrue(TEXT("计划有效"), Plan.IsValid());

	// 两条效果:一个伤害 power30,一个减速 buff(SPD,-20)
	bool bHasDamage30 = false, bHasSlow = false;
	for (const FResolvedEffectEvent& E : Plan.Effects)
	{
		TestEqual(TEXT("效果都引用 H1"), E.TargetRef, FString(TEXT("H1")));
		if (E.Effect.Type == EEffectType::Damage && E.Effect.Power == 30) bHasDamage30 = true;
		if (E.Effect.Type == EEffectType::Buff && E.Effect.Attr == FName(TEXT("SPD"))
			&& FMath::IsNearlyEqual(E.Effect.Value, -20.f)) bHasSlow = true;
	}
	TestTrue(TEXT("含伤害30效果"), bHasDamage30);
	TestTrue(TEXT("含减速SPD-20效果"), bHasSlow);

	return true;
}

// ============================================================
// Tag 门控:CanCast(沉默/MP/requireTags/blockTags)(阶段4)
// ============================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillGatingTest,
	"AiGame.Battle.SkillGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillGatingTest::RunTest(const FString& Parameters)
{
	UCombatStatusComponent* CS = NewObject<UCombatStatusComponent>(GetTransientPackage());
	FSkillEffectPlan P;
	P.MpCost = 12;
	FString R;

	// MP 充足 → 可放
	TestTrue(TEXT("MP足可放"), FSkillEffectResolver::CanCast(P, CS, 20.f, R));
	// MP 不足 → 禁
	TestFalse(TEXT("MP不足禁"), FSkillEffectResolver::CanCast(P, CS, 5.f, R));
	TestEqual(TEXT("原因=MP不足"), R, FString(TEXT("MP不足")));

	// requireTags 缺失 → 禁;具备 → 可放
	P.RequireTags = { TEXT("empowered") };
	TestFalse(TEXT("缺require禁"), FSkillEffectResolver::CanCast(P, CS, 20.f, R));
	CS->AddTag(FName(TEXT("empowered")), 2);
	TestTrue(TEXT("有require可放"), FSkillEffectResolver::CanCast(P, CS, 20.f, R));

	// blockTags 命中 → 禁
	P.RequireTags.Reset();
	P.BlockTags = { TEXT("Frozen") };
	CS->AddTag(FName(TEXT("Frozen")), 2);
	TestFalse(TEXT("有block禁"), FSkillEffectResolver::CanCast(P, CS, 20.f, R));
	TestEqual(TEXT("原因=被限制"), R, FString(TEXT("被限制")));

	// 沉默 → 禁全部(优先于其他)
	P.BlockTags.Reset();
	CS->AddTag(CombatTag::Silenced, 1);
	TestFalse(TEXT("沉默禁全部"), FSkillEffectResolver::CanCast(P, CS, 20.f, R));
	TestEqual(TEXT("原因=沉默"), R, FString(TEXT("沉默")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
