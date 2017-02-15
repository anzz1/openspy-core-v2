#include <OS/OpenSpy.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetServer.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/gsmsalg.h>
#include <OS/legacy/enctype_shared.h>
#include <OS/legacy/helpers.h>


#include <sstream>
#include <iterator>
#include "ChatDriver.h"
#include "ChatServer.h"
#include "ChatPeer.h"
#include "IRCPeer.h"
#include "ChatBackend.h"


#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>

#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
/*
	1. implement /names
	2. implement channel modes
	3. implement user chan modes
	3. implement /part
	4. implement user modes
	5. implement channel/user keys
	6. implement /notice /atm /utm, block ctcp
	6. clean up on disconnect
	7. implement /setchanprops /setusetmode /get*
	8. implement auth
	9. implement crypt
	10. implement ssl
*/


namespace Chat {
		void IRCPeer::OnPartCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			ChatBackendTask::getQueryTask()->flagPushTask();
			ChatBackendTask::SubmitRemoveUserFromChannel(NULL, irc_peer, driver, response.channel_info);
		}
		EIRCCommandHandlerRet IRCPeer::handle_part(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFindChannel(OnPartCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnJoinCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}
			ChatBackendTask::SubmitAddUserToChannel(NULL, peer, driver, response.channel_info);
		}
		EIRCCommandHandlerRet IRCPeer::handle_join(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFind_OrCreateChannel(OnJoinCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::send_channel_topic(ChatChannelInfo channel) {
			std::ostringstream s;
			s << channel.name << " :" << channel.topic;
			send_numeric(332, s.str(), true);

			s.str("");
			s << channel.name << " " << channel.topic_setby << " " << channel.topic_seton;
			send_numeric(333, s.str(), true);
		}
		void IRCPeer::OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel) {
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " JOIN " <<  channel.name << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());

			if(user.client_id == m_client_info.client_id) {
				m_channel_list.push_back(channel.channel_id);
				send_channel_topic(channel);
			}
		}
		void IRCPeer::OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel) {
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " PART " <<  channel.name << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());

			if(user.client_id == m_client_info.client_id) {
				std::vector<int>::iterator it = std::find(m_channel_list.begin(), m_channel_list.end(), 5);
				if(it != m_channel_list.end())
				    m_channel_list.erase(it);
			}
		}
		void IRCPeer::send_channel_modes(ChatChannelInfo channel) {
		}
		void IRCPeer::send_channel_names(ChatChannelInfo channel) {

		}

		void IRCPeer::OnNamesCmd_FindUsersCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			std::ostringstream s;
			s << "= " << request.query_data.channel_info.name << " :";

			std::vector<ChatChanClientInfo>::const_iterator it = response.m_channel_clients.begin();
			while(it != response.m_channel_clients.end()) {
				const ChatChanClientInfo chan_client_info = *it;
				s << chan_client_info.client_info.name;
				it++;
				if(it != response.m_channel_clients.end()) {
					s << " ";
				}
			}

			irc_peer->send_numeric(353, s.str(), true);
			irc_peer->send_numeric(366, "End of /NAMES list.");


		}
		void IRCPeer::OnNamesCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}

			ChatBackendTask::getQueryTask()->flagPushTask();

			if(response.channel_info.channel_id == 0) {

			} else {
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitGetChannelUsers(OnNamesCmd_FindUsersCallback, irc_peer, driver, response.channel_info);
			}
		}

		EIRCCommandHandlerRet IRCPeer::handle_names(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFindChannel(OnNamesCmd_FindChannelCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}

		void IRCPeer::parse_channel_modes(std::string mode_str, uint32_t &add_mask, uint32_t &remove_mask, std::back_insert_iterator<std::vector<char> > it) {
			bool add = true;
			uint32_t flag;
			for(int i=0;i<mode_str.length();i++) {
				flag = 0;
				switch(mode_str[i]) {
					case '-':
						add = false;
						break;
					case '+':
						add = true;
						break;
					case 'p':
						flag = EChannelModeFlags_Private;
					break;
					case 's':
						flag = EChannelModeFlags_Secret;
					break;
					case 'u':
						flag = EChannelModeFlags_Auditormium;
					break;
					case 'q':
						flag = EChannelModeFlags_Auditormium;
					break;
					case 'n':
						flag = EChannelModeFlags_NoOutsideMsgs;
					break;
					case 'i':
						flag = EChannelModeFlags_InviteOnly;
					break;
					case 'Z':
						flag = EChannelModeFlags_OnlyOwner;
					break;
					case 'r':
						flag = EChannelModeFlags_StayOpened;
					break;
					case 'm':
						flag = EChannelModeFlags_Moderated;
					break;
					default:
						*(it++) = mode_str[i];
					break;
				}
				if(add) {
					add_mask |= flag;
				} else {
					remove_mask |= flag;
				}
			}
		}
		void IRCPeer::OnRecvChannelModeUpdate(ChatClientInfo user, ChatChannelInfo channel, ChanModeChangeData change_data) {
			int old_modeflags = change_data.old_modeflags;

			int set_flags = (old_modeflags ^ channel.modeflags) & ~old_modeflags;
			int unset_flags = (old_modeflags & ~channel.modeflags) & old_modeflags;

			printf("Old: %d Mode: %d\n", old_modeflags, channel.modeflags);

			std::ostringstream mode_add_str;
			std::ostringstream mode_str;
			mode_add_str << "+";
			if(set_flags & EChannelModeFlags_Private) {
				mode_add_str << "p";
			}
			if(set_flags & EChannelModeFlags_Secret) {
				mode_add_str << "s";
			}
			if(set_flags & EChannelModeFlags_InviteOnly) {
				mode_add_str << "i";
			}
			if(set_flags & EChannelModeFlags_TopicOpOnly) {
				mode_add_str << "t";
			}
			if(set_flags & EChannelModeFlags_NoOutsideMsgs) {
				mode_add_str << "n";
			}
			if(set_flags & EChannelModeFlags_Moderated) {
				mode_add_str << "m";
			}
			if(set_flags & EChannelModeFlags_Auditormium) {
				mode_add_str << "u";
			}
			if(set_flags & EChannelModeFlags_VOPAuditormium) {
				mode_add_str << "q";
			}
			if(set_flags & EChannelModeFlags_StayOpened) {
				mode_add_str << "r";
			}
			if(set_flags & EChannelModeFlags_OnlyOwner) {
				mode_add_str << "Z";
			}

			std::ostringstream mode_del_str;
			mode_del_str << "-";
			if(unset_flags & EChannelModeFlags_Private) {
				mode_del_str << "p";
			}
			if(unset_flags & EChannelModeFlags_Secret) {
				mode_del_str << "s";
			}
			if(unset_flags & EChannelModeFlags_InviteOnly) {
				mode_del_str << "i";
			}
			if(unset_flags & EChannelModeFlags_TopicOpOnly) {
				mode_del_str << "t";
			}
			if(unset_flags & EChannelModeFlags_NoOutsideMsgs) {
				mode_del_str << "n";
			}
			if(unset_flags & EChannelModeFlags_Moderated) {
				mode_del_str << "m";
			}
			if(unset_flags & EChannelModeFlags_Auditormium) {
				mode_del_str << "u";
			}
			if(unset_flags & EChannelModeFlags_VOPAuditormium) {
				mode_del_str << "q";
			}
			if(unset_flags & EChannelModeFlags_StayOpened) {
				mode_del_str << "r";
			}
			if(unset_flags & EChannelModeFlags_OnlyOwner) {
				mode_del_str << "Z";
			}

			if(mode_add_str.str().length() > 1) {
				mode_str << mode_add_str.str();
			}
			if(mode_del_str.str().length() > 1) {
				mode_str << mode_del_str.str();
			}
			std::ostringstream s;

			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " MODE " << channel.name << " " << mode_str.str() << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
		}
		void IRCPeer::OnChannelTopicUpdate(ChatClientInfo user, ChatChannelInfo channel) {
			printf("Set topic to: %s\n", channel.topic.c_str());
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " TOPIC " << channel.name << " :" << channel.topic << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
		}
		void IRCPeer::OnModeCmd_ChannelUpdateCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

		}
		EIRCCommandHandlerRet IRCPeer::handle_mode(std::vector<std::string> params, std::string full_params) {
			std::string target, mode_str;
			if(params.size() > 2) {
				target = params[1];
				mode_str = params[2];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			uint32_t addmask = 0, removemask = 0;
			ChatChannelInfo channel;
			if(is_channel_name(target)) {
				std::vector<char> bad_modes;
				parse_channel_modes(params[2], addmask, removemask, std::back_inserter(bad_modes));
				channel.name = target;
				channel.channel_id = 0;
				ChatBackendTask::SubmitUpdateChannelModes(OnModeCmd_ChannelUpdateCallback, this, mp_driver, addmask, removemask, channel);
			} else {
			}
			return EIRCCommandHandlerRet_NoError;	
		}
		EIRCCommandHandlerRet IRCPeer::handle_topic(std::vector<std::string> params, std::string full_params) {
			std::string target, topic;
			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(params.size() > 2) {
				target = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			if(beg) {
				topic = (const char *)(beg+1);
				printf("The topic is: %s\n",topic.c_str());
			}
			ChatChannelInfo channel;
			if(is_channel_name(target)) {
				channel.name = target;
				channel.channel_id = 0;
				ChatBackendTask::SubmitUpdateChannelTopic(OnModeCmd_ChannelUpdateCallback, this, mp_driver, channel, topic);
			} else {
				//send error isn't channel
			}
			return EIRCCommandHandlerRet_NoError;	
		}
}