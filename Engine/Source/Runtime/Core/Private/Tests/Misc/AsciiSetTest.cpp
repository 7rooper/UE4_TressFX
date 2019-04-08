// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS 

#include "Misc/AsciiSet.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TAsciiSetTest, "System.Core.Misc.AsciiSet", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool TAsciiSetTest::RunTest(const FString& Parameters)
{
	constexpr FAsciiSet Whitespaces(" \v\f\t\r\n");
	TestTrue(TEXT("Contains"), Whitespaces.Test(' '));
	TestTrue(TEXT("Contains"), Whitespaces.Test('\n'));
	TestFalse(TEXT("Contains"), Whitespaces.Test('a'));

	constexpr FAsciiSet NonWhitespaces = ~Whitespaces;
	uint32 WhitespaceNum = 0;
	for (uint8 Char = 0; Char < 128; ++Char)
	{
		WhitespaceNum += !!Whitespaces.Test(Char);
		TestEqual(TEXT("Inverse"), !!Whitespaces.Test(Char), !NonWhitespaces.Test(Char));
	}
	TestEqual(TEXT("Num"), WhitespaceNum, 6);

	TestEqual(TEXT("Skip"), FAsciiSet::Skip(TEXT("  \t\tHello world!"), Whitespaces), TEXT("Hello world!"));
	TestEqual(TEXT("Find"), *FAsciiSet::Find("NonWhitespace\t NonWhitespace", Whitespaces), '\t');

	constexpr FAsciiSet XmlEscapeChars("&<>\"'");
	TestTrue(TEXT("None"), FAsciiSet::HasNone("No escape chars", XmlEscapeChars));
	TestFalse(TEXT("Any"), FAsciiSet::HasAny("No escape chars", XmlEscapeChars));
	TestFalse(TEXT("Only"), FAsciiSet::HasOnly("No escape chars", XmlEscapeChars));

	TestTrue(TEXT("None"), FAsciiSet::HasNone("", XmlEscapeChars));
	TestFalse(TEXT("Any"), FAsciiSet::HasAny("", XmlEscapeChars));
	TestTrue(TEXT("Only"), FAsciiSet::HasOnly("", XmlEscapeChars));

	TestFalse(TEXT("None"), FAsciiSet::HasNone("&<>\"'", XmlEscapeChars));
	TestTrue(TEXT("Any"), FAsciiSet::HasAny("&<>\"'", XmlEscapeChars));
	TestTrue(TEXT("Only"), FAsciiSet::HasOnly("&<>\"'", XmlEscapeChars));

	TestFalse(TEXT("None"), FAsciiSet::HasNone("&<>\"' and more", XmlEscapeChars));
	TestTrue(TEXT("Any"), FAsciiSet::HasAny("&<>\"' and more", XmlEscapeChars));
	TestFalse(TEXT("Only"), FAsciiSet::HasOnly("&<>\"' and more", XmlEscapeChars));

	constexpr FAsciiSet Abc("abc");
	constexpr FAsciiSet Abcd = Abc + 'd';
	TestTrue(TEXT("Add"), Abcd.Test('a'));
	TestTrue(TEXT("Add"), Abcd.Test('b'));
	TestTrue(TEXT("Add"), Abcd.Test('c'));
	TestTrue(TEXT("Add"), Abcd.Test('d'));
	TestFalse(TEXT("Add"), Abcd.Test('e'));

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS