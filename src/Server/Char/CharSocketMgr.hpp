/***************************************************
 *       _   _            _                        *
 *      | | | |          (_)                       *
 *      | |_| | ___  _ __ _ _______  _ __          *
 *      |  _  |/ _ \| '__| |_  / _ \| '_  \        *
 *      | | | | (_) | |  | |/ / (_) | | | |        *
 *      \_| |_/\___/|_|  |_/___\___/|_| |_|        *
 ***************************************************
 * This file is part of Horizon (c).
 * Copyright (c) 2018 Horizon Dev Team.
 *
 * Base Author - Sagun Khosla. (sagunxp@gmail.com)
 *
 * Under a proprietary license this file is not for use
 * or viewing without permission.
 **************************************************/

#ifndef HORIZON_CHARSOCKETMGR_H
#define HORIZON_CHARSOCKETMGR_H

#include "Core/Networking/SocketMgr.hpp"

/**
 * Auth Socket Manager
 * @brief Singleton class
 */
class CharSocketMgr : public SocketMgr<CharSession>
{
	typedef SocketMgr<CharSession> BaseSocketMgr;

public:
	static CharSocketMgr &Instance()
	{
		static CharSocketMgr instance;
		return instance;
	}

	bool StartNetwork(boost::asio::io_service &io_service, std::string const &listen_ip, uint16_t port, int threads = 1) override
	{

		if (!BaseSocketMgr::StartNetwork(io_service, listen_ip, port, threads))
			return false;

		CoreLog->trace("Max allowed socket connections {}", (int) boost::asio::socket_base::max_connections);

		_acceptor->SetSocketFactory(std::bind(&BaseSocketMgr::GetSocketForAccept, this));
		_acceptor->AsyncAcceptWithCallback<&CharSocketMgr::OnSocketAccept>();

		CoreLog->info("Networking initialized, listening on {} {} (Maximum Threads: {})", listen_ip, port, threads);

		return true;
	}

protected:
	NetworkThread<CharSession>* CreateThreads() const override
	{
		return new NetworkThread<CharSession>[GetNetworkThreadCount()];
	}

	static void OnSocketAccept(tcp::socket &&socket, uint32_t threadIndex)
	{
		Instance().OnSocketOpen(std::forward<tcp::socket>(socket), threadIndex);
	}
};

#define sCharSocketMgr CharSocketMgr::Instance()

#endif //HORIZON_CHARSOCKETMGR_H
