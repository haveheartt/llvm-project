"""
Test that the debugger handles loops in std::list (which can appear as a result of e.g. memory
corruption).
"""


import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class GenericListDataFormatterTestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def do_test_with_run_command(self):
        exe = self.getBuildArtifact("a.out")
        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target and target.IsValid(), "Target is valid")

        file_spec = lldb.SBFileSpec("main.cpp", False)
        breakpoint1 = target.BreakpointCreateBySourceRegex(
            "// Set break point at this line.", file_spec
        )
        self.assertTrue(breakpoint1 and breakpoint1.IsValid())
        breakpoint2 = target.BreakpointCreateBySourceRegex(
            "// Set second break point at this line.", file_spec
        )
        self.assertTrue(breakpoint2 and breakpoint2.IsValid())

        # Run the program, it should stop at breakpoint 1.
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        self.assertTrue(process and process.IsValid(), PROCESS_IS_VALID)
        self.assertEqual(
            len(lldbutil.get_threads_stopped_at_breakpoint(process, breakpoint1)), 1
        )

        # verify our list is displayed correctly
        self.expect(
            "frame variable numbers_list",
            substrs=["[0] = 1", "[1] = 2", "[2] = 3", "[3] = 4", "[5] = 6"],
        )

        # Continue to breakpoint 2.
        process.Continue()
        self.assertTrue(process and process.IsValid(), PROCESS_IS_VALID)
        self.assertEqual(
            len(lldbutil.get_threads_stopped_at_breakpoint(process, breakpoint2)), 1
        )

        # The list is now inconsistent. However, we should be able to get the first three
        # elements at least (and most importantly, not crash).
        self.expect(
            "frame variable numbers_list", substrs=["[0] = 1", "[1] = 2", "[2] = 3"]
        )

        # Run to completion.
        process.Continue()
        self.assertState(process.GetState(), lldb.eStateExited, PROCESS_EXITED)

    @add_test_categories(["libstdcxx"])
    def test_with_run_command_libstdcpp(self):
        self.build(dictionary={"USE_LIBSTDCPP": 1})
        self.do_test_with_run_command()

    @add_test_categories(["libc++"])
    def test_with_run_command_libcpp(self):
        self.build(dictionary={"USE_LIBCPP": 1})
        self.do_test_with_run_command()

    @add_test_categories(["msvcstl"])
    def test_with_run_command_msvcstl(self):
        # No flags, because the "msvcstl" category checks that the MSVC STL is used by default.
        self.build()
        self.do_test_with_run_command()
