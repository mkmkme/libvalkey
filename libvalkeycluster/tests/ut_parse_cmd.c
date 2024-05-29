/* Some unit tests that don't require Valkey to be running. */

#include "command.h"
#include "vkarray.h"
#include "valkeycluster.h"
#include "test_utils.h"
#include "win32.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper for the macro ASSERT_KEYS below. */
void check_keys(char **keys, int numkeys, struct cmd *command, char *file,
                int line) {
    if (command->result != CMD_PARSE_OK) {
        fprintf(stderr, "%s:%d: Command parsing failed: %s\n", file, line,
                command->errstr);
        assert(0);
    }
    int actual_numkeys = (int)vkarray_n(command->keys);
    if (actual_numkeys != numkeys) {
        fprintf(stderr, "%s:%d: Expected %d keys but got %d\n", file, line,
                numkeys, actual_numkeys);
        assert(actual_numkeys == numkeys);
    }
    for (int i = 0; i < numkeys; i++) {
        struct keypos *kpos = vkarray_get(command->keys, i);
        char *actual_key = kpos->start;
        int actual_keylen = (int)(kpos->end - kpos->start);
        if ((int)strlen(keys[i]) != actual_keylen ||
            strncmp(keys[i], actual_key, actual_keylen)) {
            fprintf(stderr,
                    "%s:%d: Expected key %d to be \"%s\" but got \"%.*s\"\n",
                    file, line, i, keys[i], actual_keylen, actual_key);
            assert(0);
        }
    }
}

/* Checks that a command (struct cmd *) has the given keys (strings). */
#define ASSERT_KEYS(command, ...)                                              \
    do {                                                                       \
        char *expected_keys[] = {__VA_ARGS__};                                 \
        size_t n = sizeof(expected_keys) / sizeof(char *);                     \
        check_keys(expected_keys, n, command, __FILE__, __LINE__);             \
    } while (0)

void test_valkey_parse_error_nonresp(void) {
    struct cmd *c = command_get();
    c->cmd = strdup("+++Not RESP+++\r\n");
    c->clen = strlen(c->cmd);

    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_ERROR, "Unexpected parse success");
    ASSERT_MSG(!strcmp(c->errstr, "Command parse error"), c->errstr);
    command_destroy(c);
}

void test_valkey_parse_cmd_get(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "GET foo");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_OK, "Parse not OK");
    ASSERT_KEYS(c, "foo");
    command_destroy(c);
}

void test_valkey_parse_cmd_mset(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "MSET foo val1 bar val2");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_OK, "Parse not OK");
    ASSERT_KEYS(c, "foo", "bar");
    command_destroy(c);
}

void test_valkey_parse_cmd_eval_1(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "EVAL dummyscript 1 foo");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_OK, "Parse not OK");
    ASSERT_KEYS(c, "foo");
    command_destroy(c);
}

void test_valkey_parse_cmd_eval_0(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "EVAL dummyscript 0 foo");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_OK, "Parse not OK");
    ASSERT_MSG(vkarray_n(c->keys) == 0, "Nonzero number of keys");
    command_destroy(c);
}

void test_valkey_parse_cmd_xgroup_no_subcommand(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "XGROUP");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_ERROR, "Unexpected parse success");
    ASSERT_MSG(!strcmp(c->errstr, "Unknown command XGROUP"), c->errstr);
    command_destroy(c);
}

void test_valkey_parse_cmd_xgroup_destroy_no_key(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "xgroup destroy");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_MSG(c->result == CMD_PARSE_ERROR, "Parse not OK");
    const char *expected_error =
        "Failed to find keys of command XGROUP DESTROY";
    ASSERT_MSG(!strncmp(c->errstr, expected_error, strlen(expected_error)),
               c->errstr);
    command_destroy(c);
}

void test_valkey_parse_cmd_xgroup_destroy_ok(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "xgroup destroy mystream mygroup");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "mystream");
    command_destroy(c);
}

void test_valkey_parse_cmd_xreadgroup_ok(void) {
    struct cmd *c = command_get();
    /* Use group name and consumer name "streams" just to try to confuse the
     * parser. The parser shouldn't mistake those for the STREAMS keyword. */
    int len = valkeyFormatCommand(
        &c->cmd, "XREADGROUP GROUP streams streams COUNT 1 streams mystream >");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "mystream");
    command_destroy(c);
}

void test_valkey_parse_cmd_xread_ok(void) {
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd,
                                 "XREAD BLOCK 42 STREAMS mystream another $ $");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "mystream");
    command_destroy(c);
}

void test_valkey_parse_cmd_restore_ok(void) {
    /* The ordering of RESTORE and RESTORE-ASKING in the lookup-table was wrong
     * in a previous version, leading to the command not being found. */
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "restore k 0 xxx");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "k");
    command_destroy(c);
}

void test_valkey_parse_cmd_restore_asking_ok(void) {
    /* The ordering of RESTORE and RESTORE-ASKING in the lookup-table was wrong
     * in a previous version, leading to the command not being found. */
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "restore-asking k 0 xxx");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "k");
    command_destroy(c);
}

void test_valkey_parse_cmd_georadius_ro_ok(void) {
    /* The position of GEORADIUS_RO was wrong in a previous version of the
     * lookup-table, leading to the command not being found. */
    struct cmd *c = command_get();
    int len = valkeyFormatCommand(&c->cmd, "georadius_ro k 0 0 0 km");
    ASSERT_MSG(len >= 0, "Format command error");
    c->clen = len;
    valkey_parse_cmd(c);
    ASSERT_KEYS(c, "k");
    command_destroy(c);
}

int main(void) {
    test_valkey_parse_error_nonresp();
    test_valkey_parse_cmd_get();
    test_valkey_parse_cmd_mset();
    test_valkey_parse_cmd_eval_1();
    test_valkey_parse_cmd_eval_0();
    test_valkey_parse_cmd_xgroup_no_subcommand();
    test_valkey_parse_cmd_xgroup_destroy_no_key();
    test_valkey_parse_cmd_xgroup_destroy_ok();
    test_valkey_parse_cmd_xreadgroup_ok();
    test_valkey_parse_cmd_xread_ok();
    test_valkey_parse_cmd_restore_ok();
    test_valkey_parse_cmd_restore_asking_ok();
    test_valkey_parse_cmd_georadius_ro_ok();
    return 0;
}
