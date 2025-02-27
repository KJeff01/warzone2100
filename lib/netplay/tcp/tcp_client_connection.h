/*
	This file is part of Warzone 2100.
	Copyright (C) 2024  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "lib/netplay/client_connection.h"
#include "lib/netplay/descriptor_set.h"
#include "lib/netplay/tcp/netsocket.h" // for SOCKET

class WzConnectionProvider;

namespace tcp
{

struct Socket;

class TCPClientConnection : public IClientConnection
{
public:

	explicit TCPClientConnection(WzConnectionProvider& connProvider, Socket* rawSocket);
	virtual ~TCPClientConnection() override;

	virtual net::result<ssize_t> readAll(void* buf, size_t size, unsigned timeout) override;
	virtual net::result<ssize_t> sendImpl(const std::vector<uint8_t>& data) override;
	virtual net::result<ssize_t> recvImpl(char* dst, size_t maxSize) override;

	virtual void setReadReady(bool ready) override;
	virtual bool readReady() const override;

	virtual void useNagleAlgorithm(bool enable) override;
	virtual std::string textAddress() const override;

	virtual bool isValid() const override;
	virtual net::result<void> connectionStatus() const override;

	SOCKET getRawSocketFd() const;

private:

	friend class TCPConnectionPollGroup;

	Socket* socket_;

	// Pre-allocated (in ctor) connection list and descriptor sets, which
	// only contain `this`.
	//
	// Used for some operations which may use polling internally
	// (like `readAll()` and `connectionStatus()`) to avoid extra
	// memory allocations.
	const std::vector<IClientConnection*> selfConnList_;
	WzConnectionProvider* connProvider_;
	std::unique_ptr<IDescriptorSet> readAllDescriptorSet_;
	std::unique_ptr<IDescriptorSet> connStatusDescriptorSet_;
};

} // namespace tcp
