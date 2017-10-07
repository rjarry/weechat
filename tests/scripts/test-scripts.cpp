/*
 * test-scripts.cpp - test scripting API
 *
 * Copyright (C) 2017 Sébastien Helleu <flashcode@flashtux.org>
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CppUTest/TestHarness.h"

extern "C"
{
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif
#include <stdio.h>
#include <string.h>
#include "src/core/weechat.h"
#include "src/core/wee-string.h"
#include "src/core/wee-hook.h"
#include "src/plugins/plugin.h"
}

extern void run_cmd (const char *command);

struct t_hook *api_hook_print = NULL;
int api_tests_ok = 0;
int api_tests_errors = 0;
int api_tests_count = 0;
int api_tests_end = 0;
int api_tests_other = 0;


TEST_GROUP(Scripts)
{
    /*
     * Callback for any message displayed by WeeChat or a plugin.
     */

    static int
    test_print_cb (const void *pointer, void *data, struct t_gui_buffer *buffer,
                   time_t date, int tags_count, const char **tags, int displayed,
                   int highlight, const char *prefix, const char *message)
    {
        const char *pos;
        char *error;
        int value;

        /* make C++ compiler happy */
        (void) pointer;
        (void) data;
        (void) buffer;
        (void) date;
        (void) tags_count;
        (void) tags;
        (void) displayed;
        (void) highlight;
        (void) prefix;

        if (message)
        {
            pos = strstr (message, "> TESTS: ");
            if (pos)
            {
                error = NULL;
                value = (int)strtol (pos + 9, &error, 10);
                if (error && !error[0])
                    api_tests_count = value;
            }
            else if (strstr (message, "TEST OK"))
                api_tests_ok++;
            else if (strstr (message, "ERROR"))
                api_tests_errors++;
            else if (strstr (message, "TESTS END"))
                api_tests_end++;
            else if ((message[0] != '>') && (message[0] != ' '))
                api_tests_other++;
        }

        return WEECHAT_RC_OK;
    }

    void setup()
    {
        /*
         * TODO: fix memory leaks in javascript plugin
         * and remove this function
         */
        MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();

        api_hook_print = hook_print (NULL,  /* plugin */
                                     NULL,  /* buffer */
                                     NULL,  /* tags */
                                     NULL,  /* message */
                                     1,     /* strip colors */
                                     &test_print_cb,
                                     NULL,
                                     NULL);
    }

    void teardown()
    {
        unhook (api_hook_print);

        /*
         * TODO: fix memory leaks in javascript plugin
         * and remove this function
         */
        MemoryLeakWarningPlugin::turnOnNewDeleteOverloads();
    }
};

/*
 * Tests scripting API.
 */

TEST(Scripts, API)
{
    char path_testapigen[PATH_MAX], path_testapi[PATH_MAX];
    char *path_testapi_output_dir, str_command[4096];
    const char *languages[][2] = {
        { "python",    "py"  },
        { "perl",      "pl"  },
        { "ruby",      "rb"  },
        { "lua",       "lua" },
        { "tcl",       "tcl" },
        { "scm",       "scm" },
        { "jvascript", "js"  },
        { "php",       "php" },
        { NULL,        NULL  }
    };
    int i;

    printf ("...\n");

    /* build paths for scripting API tests */
    snprintf (path_testapigen, sizeof (path_testapigen),
              "%s%s%s",
              getenv ("WEECHAT_TESTS_SCRIPTS_DIR"),
              DIR_SEPARATOR,
              "testapigen.py");
    snprintf (path_testapi, sizeof (path_testapi),
              "%s%s%s",
              getenv ("WEECHAT_TESTS_SCRIPTS_DIR"),
              DIR_SEPARATOR,
              "testapi.py");
    path_testapi_output_dir = string_eval_path_home ("%h/testapi",
                                                     NULL, NULL, NULL);
    CHECK(path_testapi_output_dir);

    api_tests_ok = 0;
    api_tests_errors = 0;

    /* load generator script */
    snprintf (str_command, sizeof (str_command),
              "/script load %s", path_testapigen);
    run_cmd (str_command);

    /* generate scripts to test API */
    snprintf (str_command, sizeof (str_command),
              "/testapigen %s %s",
              path_testapi,
              path_testapi_output_dir);
    run_cmd (str_command);

    /* check that there was no errors in script generation */
    LONGS_EQUAL(0, api_tests_errors);

    /* unload generator scritp */
    snprintf (str_command, sizeof (str_command),
              "/script unload testapigen.py");
    run_cmd (str_command);

    /* test the scripting API */
    for (i = 0; languages[i][0]; i++)
    {
        api_tests_ok = 0;
        api_tests_errors = 0;
        api_tests_count = 0;
        api_tests_end = 0;
        api_tests_other = 0;

        /* load script (run tests) */
        snprintf (str_command, sizeof (str_command),
                  "/script load -q %s/testapi.%s",
                  path_testapi_output_dir,
                  languages[i][1]);
        run_cmd (str_command);

        /* display results */
        printf ("\n");
        printf (">>> Tests %s: %d tests, %d OK, %d errors, "
                "%d unexpected messages\n",
                languages[i][0],
                api_tests_count,
                api_tests_ok,
                api_tests_errors,
                api_tests_other);
        printf ("\n");

        /* unload script */
        snprintf (str_command, sizeof (str_command),
                  "/script unload -q testapi.%s",
                  languages[i][1]);
        run_cmd (str_command);

        /* check that tests were found in script */
        CHECK(api_tests_count > 0);

        /* check that all tests are OK */
        LONGS_EQUAL(api_tests_count, api_tests_ok);

        /* check that there was no errors */
        LONGS_EQUAL(0, api_tests_errors);

        /* check that end of script was reached (no syntax error) */
        LONGS_EQUAL(1, api_tests_end);

        /*
         * check that there was no warning/error from plugin
         * (if everything is OK, there are 2 messages when the script is loaded
         * and 2 messages when it is unloaded, so total is 4)
         */
        LONGS_EQUAL(0, api_tests_other);
    }

    free (path_testapi_output_dir);

    printf ("TEST(Scripts, API)");
}