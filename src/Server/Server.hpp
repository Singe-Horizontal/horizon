//
//  server.hpp
//  Horizon
//
//  Created by SagunKho on 11/06/2017.
//  Copyright © 2017 Horizon. All rights reserved.
//

#ifndef HORIZON_SERVER_HPP
#define HORIZON_SERVER_HPP

#include "Logging/Logger.hpp"

#include <cstdio>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "Core/Database/MySqlConnection.hpp"

using boost::asio::ip::tcp;

struct network_configuration {
	std::string listen_ip       = "localhost";        ///< Default listening IP address.
	uint16_t listen_port        = 3306;               ///< Default listening Port.
	uint32_t max_threads    = 1;                  ///< Default Threads (Cannot be less than 1)
};

class Server
{
public:
	Server(std::string name, std::string config_file_path, std::string config_file_name);
	~Server();

	void ParseRuntimeArguments(const char *argv[], int argc);

	// Test Run
	void setTestRun() { this->general_config.minimal = true; }
	bool isTestRun() { return this->general_config.minimal; }

	// Database
	void setDBHost(std::string &host) { this->general_config.database.host = host; }
	void setDBUsername(std::string &username) { this->general_config.database.username = username; }
	void setDBPassword(std::string &password) { this->general_config.database.password = password; }
	void setDBDatabase(std::string &database) { this->general_config.database.database = database; }

	std::string &getDBHost() { return this->general_config.database.host; }
	std::string &getDBUsername() { return this->general_config.database.username; }
	std::string &getDBPassword() { return this->general_config.database.password; }
	std::string &getDBDatabase() { return this->general_config.database.database; }

	struct network_configuration &getNetConf() { return this->general_config.network; }

protected:
	/* Configuration */
	struct {
		bool minimal;
		/* Network Configuration */
		struct network_configuration network;                            ///< Network Configuration information.
		/* Configuration Files */
		std::string config_file_path;
		std::string config_file_name;                                    ///< Configuration File name.
		/**
		 * Database
		 */
		struct {
			std::string host, username, password;
			std::string database;
		} database;
	} general_config;

	/* MySQL Connection Factory & Pool */
	boost::shared_ptr<MySQLConnectionFactory> mysql_connection_factory;
	boost::shared_ptr<ConnectionPool<MySQLConnection>> mysql_pool;
};

#endif /* HORIZON_SERVER_HPP */
