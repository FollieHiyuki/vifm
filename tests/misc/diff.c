#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stddef.h> /* NULL */
#include <stdio.h> /* EOF FILE fclose() fgetc() fopen() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/compare.h"
#include "../../src/ops.h"
#include "../../src/undo.h"

#include "utils.h"

static int exec_func(OPS op, void *data, const char *src, const char *dst);
static int op_avail(OPS op);
static int files_are_identical(const char a[], const char b[]);

SETUP()
{
	static int max_undo_levels = 0;

	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	opt_handlers_setup();

	cfg.delete_prg = strdup("");
	cfg.use_system_calls = 1;
	init_undo_list(&exec_func, &op_avail, NULL, &max_undo_levels);
}

static int
exec_func(OPS op, void *data, const char *src, const char *dst)
{
	return 0;
}

static int
op_avail(OPS op)
{
	return 0;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	opt_handlers_teardown();

	free(cfg.delete_prg);
}

TEST(moving_does_not_work_in_non_diff)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/a");
	strcpy(rwin.curr_dir, TEST_DATA_PATH "/compare/b");
	(void)compare_one_pane(&lwin, CT_CONTENTS, LT_ALL, 0);

	(void)compare_move(&lwin, &rwin);

	assert_true(files_are_identical(
				TEST_DATA_PATH "/compare/a/same-content-different-name-1",
				TEST_DATA_PATH "/compare/b/same-content-different-name-1"));
}

TEST(moving_fake_entry_removes_the_other_file)
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	create_file(SANDBOX_PATH "/empty");

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 4;
	lwin.list_pos = 4;
	(void)compare_move(&lwin, &rwin);

	assert_failure(remove(SANDBOX_PATH "/empty"));
}

TEST(moving_mismatched_entry_makes_files_equal)
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-different-content");

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 2;
	lwin.list_pos = 2;
	(void)compare_move(&lwin, &rwin);

	assert_true(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	assert_success(remove(SANDBOX_PATH "/same-name-different-content"));
}

TEST(moving_equal_does_nothing)
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	copy_file(TEST_DATA_PATH "/compare/a/same-name-same-content",
			SANDBOX_PATH "/same-name-same-content");

	assert_true(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);

	/* Replace file unbeknownst to main code. */
	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-same-content");
	assert_false(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	rwin.list_pos = 3;
	lwin.list_pos = 3;
	(void)compare_move(&lwin, &rwin);

	/* Check that file wasn't replaced. */
	assert_false(files_are_identical(SANDBOX_PATH "/same-name-same-content",
				TEST_DATA_PATH "/compare/b/same-name-same-content"));

	assert_success(remove(SANDBOX_PATH "/same-name-same-content"));
}

TEST(file_id_is_not_updated_on_failed_move, IF(not_windows))
{
	strcpy(rwin.curr_dir, SANDBOX_PATH);
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/compare/b");

	copy_file(TEST_DATA_PATH "/compare/a/same-name-different-content",
			SANDBOX_PATH "/same-name-different-content");

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));

	(void)compare_two_panes(CT_CONTENTS, LT_ALL, 1, 0);
	rwin.list_pos = 2;
	lwin.list_pos = 2;

	assert_success(chmod(SANDBOX_PATH, 0000));
	(void)compare_move(&lwin, &rwin);
	assert_success(chmod(SANDBOX_PATH, 0777));

	assert_false(files_are_identical(SANDBOX_PATH "/same-name-different-content",
				TEST_DATA_PATH "/compare/b/same-name-different-content"));
	assert_false(lwin.dir_entry[2].id == rwin.dir_entry[2].id);

	assert_success(remove(SANDBOX_PATH "/same-name-different-content"));
}

static int
files_are_identical(const char a[], const char b[])
{
	FILE *const a_file = fopen(a, "rb");
	FILE *const b_file = fopen(b, "rb");
	int a_data, b_data;

	if(a_file == NULL || b_file == NULL)
	{
		if(a_file != NULL)
		{
			fclose(a_file);
		}
		if(b_file != NULL)
		{
			fclose(b_file);
		}
		return 0;
	}

	do
	{
		a_data = fgetc(a_file);
		b_data = fgetc(b_file);
	}
	while(a_data != EOF && b_data != EOF);

	fclose(b_file);
	fclose(a_file);

	return a_data == b_data && a_data == EOF;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
