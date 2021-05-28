extern "C"
{
#include <libcomp/include/comp_opt.h>
}

#include <gmock/gmock.h>

using namespace ::testing;

TEST(CmdLineParser, test1)
{
	const char* argv[] =
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
	const char* argv[] =
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
	const char* argv[] =
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

TEST(CmdLineParser, test4)
{
	const char* argv[] =
	{
		"testapp",
		"-Evo",
		"output.asm",
		"-c",
		"config.cfg"
	};

	comp_opt_t opt = parse_command_line(5, argv);
	EXPECT_EQ(true, opt.valid);
	EXPECT_EQ(true, opt.display_version);
	EXPECT_EQ(true, opt.pre_proc_only);
	EXPECT_THAT(opt.output_path, StrEq("output.asm"));
	EXPECT_THAT(opt.config_path, StrEq("config.cfg"));
}

TEST(CmdLineParser, test_invalid1)
{
	const char* argv[] =
	{
		"testapp"
	};

	comp_opt_t opt = parse_command_line(1, argv);
	EXPECT_EQ(false, opt.valid);
}

TEST(CmdLineParser, test_version)
{
	const char* argv[] =
	{
		"testapp",
		"-v"
	};

	comp_opt_t opt = parse_command_line(2, argv);
	EXPECT_EQ(true, opt.valid);
}