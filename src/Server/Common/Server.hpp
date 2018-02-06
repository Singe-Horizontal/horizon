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

#ifndef HORIZON_SERVER_HPP
#define HORIZON_SERVER_HPP

#include "Logging/Logger.hpp"
#include "CLI/CLICommand.hpp"
#include "Core/Database/MySqlConnection.hpp"
#include "Core/Multithreading/LockedQueue.hpp"
#include "Models/Configuration/GeneralServerConfiguration.hpp"

#include <cstdio>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <yaml-cpp/yaml.h>

using boost::asio::ip::tcp;

class Server
{
public:
	Server(std::string name, std::string config_file_path, std::string config_file_name);
	~Server();

	void ParseExecArguments(const char *argv[], int argc);

	/* Shutdown Handlers */
	void shutdown(int signal) { getGeneralConf().setShuttingDown(signal); }
	bool IsShuttingDown() { return getGeneralConf().IsShuttingDown(); }
	void IOServiceLoop();

	/* Core I/O Service*/
	const std::shared_ptr<boost::asio::io_service> &getIOService() const;
	/* General Configuration */
	struct general_server_configuration &getGeneralConf() { return this->general_config; }
	/* Network Configuration */
	network_configuration &getNetworkConf() { return network_config; };
	void setNetwork(const network_configuration &network) { this->network_config = network; };
	/* Database Configuration */
	database_configuration &getDatabaseConf() { return database_config; }

	/**
	 * Processing Functions
	 */
	/* Common Configuration */
	bool ProcessCommonConfiguration(YAML::Node &config);
	/* Initialize Core */
	virtual void InitializeCore();
	/* Mysql Threads */
	void InitializeMySQLConnections();
	boost::shared_ptr<MySQLConnection> MySQLBorrow() { return mysql_pool->borrow(); }
	void MySQLUnborrow(boost::shared_ptr<MySQLConnection> conn) { mysql_pool->unborrow(conn); }
	/* Command Line Interface */
	void InitializeCLI();

	virtual void InitializeCLICommands();
	void InitializeCommonCLICommands();
	void ProcessCLICommands();
	void QueueCLICommand(CLICommand *cmdMgr) { m_CLICmdQueue.add(cmdMgr); }
	void addCLIFunction(std::string cmd, std::function<bool(void)> func) { m_CLIFunctionMap.insert(std::make_pair(cmd, func)); };
	
	/* CLI Function getter */
	std::function<bool(void)> getCLIFunc(std::string &cmd)
	{
		auto it = m_CLIFunctionMap.find(cmd);
		return (it != m_CLIFunctionMap.end()) ? it->second : nullptr;
	}

	/**
	 * CLI Commands
	 */
	bool CLICmd_Shutdown() { shutdown(SIGTERM); return true; }

protected:
	/* General Configuration */
	struct general_server_configuration general_config;
	/* Network Configuration */
	struct network_configuration network_config;
	/* Database Configuration */
	struct database_configuration database_config;
	/* MySQL Connection Factory & Pool */
	boost::shared_ptr<MySQLConnectionFactory> mysql_connection_factory;
	boost::shared_ptr<ConnectionPool<MySQLConnection>> mysql_pool;
	// CLI command holder to be thread safe
	LockedQueue<CLICommand *> m_CLICmdQueue;
	std::thread m_CLIThread;
	std::unordered_map<std::string, std::function<bool(void)>> m_CLIFunctionMap;
	/**
	 * Core IO Service
	 * @owner shared.
	 */
	std::shared_ptr<boost::asio::io_service> io_service;
};

#endif /* HORIZON_SERVER_HPP */
