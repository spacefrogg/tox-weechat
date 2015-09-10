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

#include <weechat/weechat-plugin.h>
#include <tox/tox.h>

#include "twc.h"
#include "twc-profile.h"
#include "twc-chat.h"
#include "twc-friend-request.h"
#include "twc-group-invite.h"
#include "twc-message-queue.h"
#include "twc-utils.h"

#include "twc-tox-callbacks.h"

int
twc_do_timer_cb(void *data,
                int remaining_calls)
{
    struct t_twc_profile *profile = data;

    tox_iterate(profile->tox);
    struct t_hook *hook = weechat_hook_timer(tox_iteration_interval(profile->tox),
                                             0, 1, twc_do_timer_cb, profile);
    profile->tox_do_timer = hook;

    // check connection status
    TOX_CONNECTION connection = tox_self_get_connection_status(profile->tox);
    bool is_connected = connection == TOX_CONNECTION_TCP
                        || connection == TOX_CONNECTION_UDP;
    twc_profile_set_online_status(profile, is_connected);

    return WEECHAT_RC_OK;
}

void
twc_friend_message_callback(Tox *tox, uint32_t friend_number,
                            TOX_MESSAGE_TYPE type,
                            const uint8_t *message, size_t length,
                            void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     true);

    char *name = twc_get_name_nt(profile->tox, friend_number);
    char *message_nt = twc_null_terminate(message, length);

    twc_chat_print_message(chat, "", name,
                           message_nt, type);

    free(name);
    free(message_nt);
}

void
twc_connection_status_callback(Tox *tox, uint32_t friend_number,
                               TOX_CONNECTION status, void *data)
{
    struct t_twc_profile *profile = data;
    char *name = twc_get_name_nt(profile->tox, friend_number);
    struct t_gui_nick *nick = weechat_nicklist_search_nick(profile->buffer,
                                                           NULL,
                                                           name);

    // TODO: print in friend's buffer if it exists
    if (status == 0)
    {
        weechat_printf(profile->buffer,
                       "%s%s just went offline.",
                       weechat_prefix("quit"),
                       name);
        if (nick)
            weechat_nicklist_nick_set(profile->buffer,
                                      nick,
                                      "visible", "0");
    }
    else if (status == 1)
    {
        weechat_printf(profile->buffer,
                       "%s%s just came online.",
                       weechat_prefix("join"),
                       name);
        if (nick)
            weechat_nicklist_nick_set(profile->buffer,
                                      nick,
                                      "visible", "1");
        twc_message_queue_flush_friend(profile, friend_number);
    }
    free(name);
}

void
twc_name_change_callback(Tox *tox, uint32_t friend_number,
                         const uint8_t *name, size_t length,
                         void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);

    char *old_name = twc_get_name_nt(profile->tox, friend_number);
    char *new_name = twc_null_terminate(name, length);
    struct t_gui_nick *old_nick = weechat_nicklist_search_nick(profile->buffer,
                                                               NULL,
                                                               old_name);
    struct t_gui_nick *new_nick = weechat_nicklist_search_nick(profile->buffer,
                                                               NULL,
                                                               new_name);

    if (strcmp(old_name, new_name) != 0)
    {
        if (chat)
        {
            twc_chat_queue_refresh(chat);

            weechat_printf(chat->buffer,
                           "%s%s is now known as %s",
                           weechat_prefix("network"),
                           old_name, new_name);
            
        }

        weechat_printf(profile->buffer,
                       "%s%s is now known as %s",
                       weechat_prefix("network"),
                       old_name, new_name);
        int visible = 0;
        if (old_nick)
        {
            weechat_nicklist_remove_nick(profile->buffer, old_nick);
            visible = weechat_nicklist_nick_get_integer(profile->buffer,
                                                        old_nick,
                                                        "visible");
        }
        if (!new_nick)
            weechat_nicklist_add_nick(profile->buffer, NULL, new_name,
                                      NULL, NULL, NULL, visible);
    }

    free(old_name);
    free(new_name);
}

void
twc_user_status_callback(Tox *tox, uint32_t friend_number,
                         TOX_USER_STATUS status, void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_status_message_callback(Tox *tox, uint32_t friend_number,
                            const uint8_t *message, size_t length,
                            void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_friend(profile,
                                                     friend_number,
                                                     false);
    if (chat)
        twc_chat_queue_refresh(chat);
}

void
twc_friend_request_callback(Tox *tox, const uint8_t *public_key,
                            const uint8_t *message, size_t length,
                            void *data)
{
    struct t_twc_profile *profile = data;

    char *message_nt = twc_null_terminate(message, length);
    int rc = twc_friend_request_add(profile, public_key, message_nt);

    if (rc == -1)
    {
        weechat_printf(profile->buffer,
                       "%sReceived a friend request, but your friend request list is full!",
                       weechat_prefix("warning"));
    }
    else
    {
        char hex_address[TOX_PUBLIC_KEY_SIZE * 2 + 1];
        twc_bin2hex(public_key, TOX_PUBLIC_KEY_SIZE, hex_address);

        weechat_printf(profile->buffer,
                       "%sReceived a friend request from %s with message \"%s\"; "
                       "accept it with \"/friend accept %d\"",
                       weechat_prefix("network"),
                       hex_address, message_nt, rc);

        if (rc == -2)
        {
            weechat_printf(profile->buffer,
                           "%sFailed to save friend request, try manually "
                           "accepting with /friend add",
                           weechat_prefix("error"));
        }
    }

    free(message_nt);
}

void
twc_group_invite_callback(Tox *tox,
                          int32_t friend_number, uint8_t type,
                          const uint8_t *invite_data, uint16_t length,
                          void *data)
{
    struct t_twc_profile *profile = data;
    char *friend_name = twc_get_name_nt(profile->tox, friend_number);

    int64_t rc = twc_group_chat_invite_add(profile, friend_number, type,
                                           (uint8_t *)invite_data, length);

    char *type_str;
    switch (type)
    {
        case TOX_GROUPCHAT_TYPE_TEXT:
            type_str = "a text-only group chat"; break;
        case TOX_GROUPCHAT_TYPE_AV:
            type_str = "an audio/video group chat"; break;
        default:
            type_str = "a group chat of unknown type"; break;
    }

    if (rc >= 0)
    {
        weechat_printf(profile->buffer,
                       "%sReceived %s invite from %s; "
                       "join with \"/group join %d\"",
                       weechat_prefix("network"), type_str, friend_name, rc);
    }
    else
    {
        weechat_printf(profile->buffer,
                       "%sReceived a group chat invite from %s, but failed to "
                       "process it; try again",
                       weechat_prefix("error"), friend_name, rc);
    }

    free(friend_name);
}

void
twc_handle_group_message(Tox *tox,
                         int32_t group_number, int32_t peer_number,
                         const uint8_t *message, uint16_t length,
                         void *data,
                         enum TWC_MESSAGE_TYPE message_type)
{
    struct t_twc_profile *profile = data;

    struct t_twc_chat *chat = twc_chat_search_group(profile,
                                                    group_number,
                                                    true);

    char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);
    char *message_nt = twc_null_terminate(message, length);

    twc_chat_print_message(chat, "", name,
                           message_nt, message_type);

    free(name);
    free(message_nt);
}

void
twc_group_message_callback(Tox *tox,
                           int32_t group_number, int32_t peer_number,
                           const uint8_t *message, uint16_t length,
                           void *data)
{
    twc_handle_group_message(tox,
                             group_number,
                             peer_number,
                             message,
                             length,
                             data,
                             TWC_MESSAGE_TYPE_MESSAGE);
}

void
twc_group_action_callback(Tox *tox,
                          int32_t group_number, int32_t peer_number,
                          const uint8_t *message, uint16_t length,
                          void *data)
{
    twc_handle_group_message(tox,
                             group_number,
                             peer_number,
                             message,
                             length,
                             data,
                             TWC_MESSAGE_TYPE_ACTION);
}

void
twc_group_namelist_change_callback(Tox *tox,
                                   int group_number, int peer_number,
                                   uint8_t change_type,
                                   void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_group(profile,
                                                    group_number,
                                                    true);

    struct t_gui_nick *nick = NULL;
    char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);
    char *prev_name = NULL;

    uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
    int pkrc = tox_group_peer_pubkey(profile->tox, group_number,
                                     peer_number, pubkey);
    if (pkrc == 0)
    {
        if (change_type == TOX_CHAT_CHANGE_PEER_DEL
            || change_type == TOX_CHAT_CHANGE_PEER_NAME)
        {
            nick = weechat_hashtable_get(chat->nicks, pubkey);
            if (nick)
            {
                prev_name = strdup(weechat_nicklist_nick_get_string(chat->buffer,
                                                                    nick, "name"));
                weechat_nicklist_remove_nick(chat->buffer, nick);
                weechat_hashtable_remove(chat->nicks, pubkey);
            }
        }

        if (change_type == TOX_CHAT_CHANGE_PEER_ADD
            || change_type == TOX_CHAT_CHANGE_PEER_NAME)
        {
            nick = weechat_nicklist_add_nick(chat->buffer, chat->nicklist_group,
                                             name, NULL, NULL, NULL, 1);
            if (nick)
                weechat_hashtable_set_with_size(chat->nicks,
                                                pubkey, TOX_PUBLIC_KEY_SIZE,
                                                nick, 0);
        }
    }

    switch (change_type)
    {
        case TOX_CHAT_CHANGE_PEER_NAME:
            if (prev_name && name)
                weechat_printf(chat->buffer, "%s%s is now known as %s",
                               weechat_prefix("network"), prev_name, name);
            break;
        case TOX_CHAT_CHANGE_PEER_ADD:
            if (name)
                weechat_printf(chat->buffer, "%s%s just joined the group chat",
                               weechat_prefix("join"), name);
            break;
        case TOX_CHAT_CHANGE_PEER_DEL:
            if (prev_name)
                weechat_printf(chat->buffer, "%s%s just left the group chat",
                               weechat_prefix("quit"), prev_name);
            break;
    }

    if (prev_name)
        free(prev_name);
}

void
twc_group_title_callback(Tox *tox,
                         int group_number, int peer_number,
                         const uint8_t *title, uint8_t length,
                         void *data)
{
    struct t_twc_profile *profile = data;
    struct t_twc_chat *chat = twc_chat_search_group(profile,
                                                    group_number,
                                                    true);
    twc_chat_queue_refresh(chat);

    if (peer_number >= 0)
    {
        char *name = twc_get_peer_name_nt(profile->tox, group_number, peer_number);

        char *topic = strndup((char *)title, length);
        weechat_printf(chat->buffer, "%s%s has changed the topic to \"%s\"",
                       weechat_prefix("network"), name, topic);
        free(topic);
    }
}

