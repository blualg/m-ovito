////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>

#include <QLibrary>

namespace Ovito {

#ifndef OVITO_LIBSSH_RUNTIME_LINKING
    #define OVITO_LIBSSH_RESOLVE_FUNCTION(funcname) \
        static funcname##_ptr funcname() { return ::funcname; }
#else
    #define OVITO_LIBSSH_RESOLVE_FUNCTION(funcname) \
        static funcname##_ptr funcname() { \
            static funcname##_ptr func = (funcname##_ptr)libssh.resolve(#funcname); \
            OVITO_ASSERT(func); \
            return func; \
        }
#endif

class LibsshWrapper
{
public:
    using ssh_channel_close_ptr = int (*)(ssh_channel channel);
    using ssh_channel_free_ptr = void (*)(ssh_channel channel);
    using ssh_channel_is_open_ptr = int (*)(ssh_channel channel);
    using ssh_channel_new_ptr = ssh_channel (*)(ssh_session session);
    using ssh_channel_open_session_ptr = int (*)(ssh_channel channel);
    using ssh_channel_poll_ptr = int (*)(ssh_channel channel, int is_stderr);
    using ssh_channel_read_nonblocking_ptr = int (*)(ssh_channel channel, void *dest, uint32_t count, int is_stderr);
    using ssh_channel_request_exec_ptr = int (*)(ssh_channel channel, const char *cmd);
    using ssh_channel_send_eof_ptr = int (*)(ssh_channel channel);
    using ssh_channel_write_ptr = int (*)(ssh_channel channel, const void *data, uint32_t len);
    using ssh_clean_pubkey_hash_ptr = void (*)(unsigned char **hash);
    using ssh_connect_ptr = int (*)(ssh_session session);
    using ssh_disconnect_ptr = void (*)(ssh_session session);
    using ssh_free_ptr = void (*)(ssh_session session);
    using ssh_get_error_ptr = const char* (*)(void *error);
    using ssh_get_error_code_ptr = int (*)(void *error);
    using ssh_get_fd_ptr = socket_t (*)(ssh_session session);
    using ssh_get_hexa_ptr = char* (*)(const unsigned char *what, size_t len);
    using ssh_get_publickey_hash_ptr = int (*)(const ssh_key key, enum ssh_publickey_hash_type type, unsigned char **hash, size_t *hlen);
    using ssh_get_server_publickey_ptr = int (*)(ssh_session session, ssh_key *key);
    using ssh_get_status_ptr = int (*)(ssh_session session);
    using ssh_is_connected_ptr = int (*)(ssh_session session);
    using ssh_session_is_known_server_ptr = enum ssh_known_hosts_e (*)(ssh_session session);
    using ssh_session_update_known_hosts_ptr = int (*)(ssh_session session);
    using ssh_key_free_ptr = void (*)(ssh_key key);
    using ssh_new_ptr = ssh_session (*)(void);
    using ssh_options_get_ptr = int (*)(ssh_session session, enum ssh_options_e type, char **value);
    using ssh_options_parse_config_ptr = int (*)(ssh_session session, const char *filename);
    using ssh_options_set_ptr = int (*)(ssh_session session, enum ssh_options_e type, const void *value);
    using ssh_set_blocking_ptr = void (*)(ssh_session session, int blocking);
    using ssh_set_callbacks_ptr = int (*)(ssh_session session, ssh_callbacks cb);
    using ssh_set_log_level_ptr = int (*)(int level);
    using ssh_set_log_callback_ptr = int (*)(ssh_logging_callback cb);
    using ssh_set_log_userdata_ptr = int (*)(void *data);
    using ssh_userauth_none_ptr = int (*)(ssh_session session, const char *username);
    using ssh_userauth_autopubkey_ptr = int (*)(ssh_session session, const char *passphrase);
    using ssh_userauth_password_ptr = int (*)(ssh_session session, const char *username, const char *password);
    using ssh_userauth_kbdint_ptr = int (*)(ssh_session session, const char *user, const char *submethods);
    using ssh_userauth_kbdint_getnprompts_ptr = int (*)(ssh_session session);
    using ssh_string_free_char_ptr = void (*)(char *s);
    using ssh_userauth_kbdint_getinstruction_ptr = const char* (*)(ssh_session session);
    using ssh_userauth_kbdint_setanswer_ptr = int (*)(ssh_session session, unsigned int i, const char *answer);
    using ssh_userauth_kbdint_getprompt_ptr = const char* (*)(ssh_session session, unsigned int i, char *echo);
    using ssh_userauth_list_ptr = int (*)(ssh_session session, const char *username);
    using ssh_userauth_publickey_auto_ptr = int (*)(ssh_session session, const char *username, const char *passphrase);
    using ssh_userauth_publickey_ptr = int (*)(ssh_session session, const char *username, const ssh_key privkey);
    using ssh_pki_import_privkey_file_ptr = int (*)(const char *filename, const char *passphrase, ssh_auth_callback auth_fn, void *auth_data, ssh_key *pkey);
    using _ssh_log_ptr = void (*)(int verbosity, const char *function, const char *format, ...);
    using ssh_set_channel_callbacks_ptr = int (*)(ssh_channel channel, ssh_channel_callbacks cb);
    using ssh_remove_channel_callbacks_ptr = int (*)(ssh_channel channel, ssh_channel_callbacks cb);

    /// Loads libssh into the process and resolves the function pointers.
    static void initialize();

    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_close)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_free)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_is_open)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_new)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_open_session)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_poll)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_read_nonblocking)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_request_exec)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_send_eof)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_channel_write)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_clean_pubkey_hash)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_connect)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_disconnect)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_free)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_error)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_error_code)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_fd)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_hexa)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_publickey_hash)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_server_publickey)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_get_status)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_is_connected)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_session_is_known_server)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_session_update_known_hosts)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_key_free)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_new)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_options_get)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_options_parse_config)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_options_set)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_blocking)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_callbacks)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_log_level)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_log_callback)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_log_userdata)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_none)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_autopubkey)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_password)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_kbdint)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_kbdint_getnprompts)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_string_free_char)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_kbdint_getinstruction)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_kbdint_setanswer)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_kbdint_getprompt)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_list)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_publickey_auto)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_userauth_publickey)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_pki_import_privkey_file)
    OVITO_LIBSSH_RESOLVE_FUNCTION(_ssh_log)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_set_channel_callbacks)
    OVITO_LIBSSH_RESOLVE_FUNCTION(ssh_remove_channel_callbacks)

private:

#ifdef OVITO_LIBSSH_RUNTIME_LINKING
    static QLibrary libssh;
#endif
};

#undef OVITO_LIBSSH_RESOLVE_FUNCTION

} // End of namespace
