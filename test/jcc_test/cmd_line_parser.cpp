extern "C"
{
#include <libcomp/include/comp_opt.h>
}

#include <gmock/gmock.h>

using namespace ::testing;

TEST(CmdLineParser, test1)
{
	char* argv[] =
	{
		"testapp",
		"blah.c",
		"-v",
		"-o",
		"output.asm"
	};

	comp_opt_t opt = parse_command_line(5, argv);
	EXPECT_EQ(true, opt.valid);
	EXPECT_EQ(true, opt.display_version);
	EXPECT_EQ(false, opt.pre_proc_only);
	EXPECT_THAT(opt.output_path, StrEq("output.asm"));
	EXPECT_THAT(opt.input_path, StrEq("blah.c"));
}

TEST(CmdLineParser, test2)
{
	char* argv[] =
	{
		"testapp",
		"-Evo",
		"output.asm"
	};

	comp_opt_t opt = parse_command_line(3, argv);
	EXPECT_EQ(true, opt.valid);
	EXPECT_EQ(true, opt.display_version);
	EXPECT_EQ(true, opt.pre_proc_only);
	EXPECT_THAT(opt.output_path, StrEq("output.asm"));
}

TEST(CmdLineParser, test3)
{
	char* argv[] =
	{
		"testapp",
		"-Evo",
		"output.asm"
	};

	comp_opt_t opt = parse_command_line(3, argv);
	EXPECT_EQ(true, opt.valid);
	EXPECT_EQ(true, opt.display_version);
	EXPECT_EQ(true, opt.pre_proc_only);
	EXPECT_THAT(opt.output_path, StrEq("output.asm"));
}

TEST(CmdLineParser, test_invalid1)
{
	char* argv[] =
	{
		"testapp"
	};

	comp_opt_t opt = parse_command_line(1, argv);
	EXPECT_EQ(false, opt.valid);
}

TEST(CmdLineParser, test_version)
{
	char* argv[] =
	{
		"testapp",
		"-v"
	};

	comp_opt_t opt = parse_command_line(2, argv);
	EXPECT_EQ(true, opt.valid);
}