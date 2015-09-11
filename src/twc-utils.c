/*
 * Copyright (c) 2015 Håvard Pettersson <mail@haavard.me>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-config.h"

#include "twc-utils.h"

/**
 * Convert a hex string to it's binary equivalent of max size bytes.
 */
void
twc_hex2bin(const char *hex, size_t size, uint8_t *out)
{
    const char *position = hex;

    size_t i;
    for (i = 0; i < size; ++i)
    {
        sscanf(position, "%2hhx", &out[i]);
        position += 2;
    }
}

/**
 * Convert size bytes to a hex string. out must be at lesat size * 2 + 1
 * bytes.
 */
void
twc_bin2hex(const uint8_t *bin, size_t size, char *out)
{
    char *position = out;

    size_t i;
    for (i = 0; i < size; ++i)
    {
        sprintf(position, "%02X", bin[i]);
        position += 2;
    }
    *position = 0;
}

/**
 * Return a null-terminated copy of str. Must be freed.
 */
char *
twc_null_terminate(const uint8_t *str, size_t length)
{
    char *str_null = malloc(length + 1);
    memcpy(str_null, str, length);
    str_null[length] = 0;

    return str_null;
}

/**
 * Get the null-terminated name of a Tox friend. Must be freed.
 */
char *
twc_get_name_nt(Tox *tox, int32_t friend_number)
{
    TOX_ERR_FRIEND_QUERY err;
    size_t length = tox_friend_get_name_size(tox, friend_number, &err);

    if ((err != TOX_ERR_FRIEND_QUERY_OK) ||
        (length == 0))
        return twc_get_friend_id_short(tox, friend_number);

    uint8_t name[length];

    tox_friend_get_name(tox, friend_number, name, &err);
    return twc_null_terminate(name, length);
}

/**
 * Get the null-terminated screen name of a Tox friend.
 */
char *
twc_get_screen_name_nt(struct t_twc_profile *profile, uint32_t friend_number)
{
    size_t friend_count = tox_self_get_friend_list_size(profile->tox);
    uint32_t friend_numbers[friend_count];

    if (!profile->screen_names)
        return twc_get_name_nt(profile->tox, friend_number);

    tox_self_get_friend_list(profile->tox, friend_numbers);

    for (size_t i = 0; i < friend_count; ++i)
    {
        if (friend_number == friend_numbers[i])
            return strdup(profile->screen_names[i]);
    }
    return NULL;
}

/**
 * Return the null-terminated status message of a Tox friend. Must be freed.
 */
char *
twc_get_status_message_nt(Tox *tox, int32_t friend_number)
{
    TOX_ERR_FRIEND_QUERY err;
    size_t length = tox_friend_get_status_message_size(tox, friend_number, &err);

    if ((err != TOX_ERR_FRIEND_QUERY_OK) ||
        (length == SIZE_MAX)) {
        char *msg = malloc(1);
        *msg = 0;
        return msg;
    }

    uint8_t message[length];
    tox_friend_get_status_message(tox, friend_number, message, &err);

    return twc_null_terminate(message, length);
}

/**
 * Return the name of a group chat peer as a null terminated string. Must be
 * freed.
 */
char *
twc_get_peer_name_nt(Tox *tox, int32_t group_number, int32_t peer_number)
{
    uint8_t name[TOX_MAX_NAME_LENGTH] = {0};

    int length = tox_group_peername(tox, group_number, peer_number, name);
    if (length >= 0)
        return twc_null_terminate(name, length);
    else
        return "<unknown>";
}

/**
 * Return the users own name, null-terminated. Must be freed.
 */
char *
twc_get_self_name_nt(Tox *tox)
{
    size_t length = tox_self_get_name_size(tox);
    uint8_t name[length];
    tox_self_get_name(tox, name);

    return twc_null_terminate(name, length);
}

/**
 * Return a friend's Tox ID in short form. Return value must be freed.
 */
char *
twc_get_friend_id_short(Tox *tox, int32_t friend_number)
{
    uint8_t client_id[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_FRIEND_GET_PUBLIC_KEY err;
    size_t short_id_length = weechat_config_integer(twc_config_short_id_size);
    char *hex_address = malloc(short_id_length + 1);

    tox_friend_get_public_key(tox, friend_number, client_id, &err);

    // return a zero public key on failure
    if (err != TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK)
        memset(client_id, 0, TOX_PUBLIC_KEY_SIZE);

    twc_bin2hex(client_id,
                short_id_length / 2,
                hex_address);

    return hex_address;
}

/**
 * Reverse the bytes of a 32-bit integer.
 */
uint32_t
twc_uint32_reverse_bytes(uint32_t num)
{
    uint32_t res = 0;

    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF; num >>= 8; res <<= 8;
    res += num & 0xFF;

    return res;
}

/**
 * Hash a Tox ID of size TOX_PUBLIC_KEY_SIZE bytes using a modified djb2 hash.
 */
unsigned long long
twc_hash_tox_id(const uint8_t *tox_id)
{
    unsigned long long hash = 5381;

    for (size_t i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i)
        hash ^= (hash << 5) + (hash >> 2) + tox_id[i];

    return hash;
}

char *
twc_format_nick(struct t_gui_buffer *buffer, struct t_gui_nick *nick)
{
    char *c_res = weechat_color("reset");
    const char *c_pfx;
    const char *name;
    const char *pfx;
    char *str;

    if (!nick || !buffer)
        return NULL;

    c_pfx = weechat_nicklist_nick_get_string(buffer, nick, "prefix_color");
    pfx = weechat_nicklist_nick_get_string(buffer, nick, "prefix");
    name = weechat_nicklist_nick_get_string(buffer, nick, "name");
    str = (char *)malloc(strlen(c_res) + strlen(c_pfx) + strlen(name)
                         + strlen(pfx) + 1);

    if (!str)
        return NULL;

    if (strlen(pfx) > 0)
        sprintf(str, "%s%s%s%s", c_pfx, pfx, c_res, name);
    else
        sprintf(str, "%s", name);
    return str;
}

char *
twc_unique_name(struct t_twc_profile *profile, const uint32_t friend_number)
{
    if (!profile)
        return NULL;

    char *name = twc_get_name_nt(profile->tox, friend_number);
    struct t_gui_nick *nick = weechat_nicklist_search_nick(profile->buffer, NULL, name);
    while (nick)
    {
        name = (char *)realloc(name, strlen(name) + 2);
        if (!name)
            return NULL;
        strcat(name, "_");
        nick = weechat_nicklist_search_nick(profile->buffer, NULL, name);
    }
    return name;
}
