// Copyright (c) 2015-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef EVENT_MONITOR_SERVICE_ERROR_HPP
#define EVENT_MONITOR_SERVICE_ERROR_HPP

#include <cstring>
#include <iostream>
#include <exception>

namespace EventMonitor {

/**
 * @brief This class wraps Event monitor errors.
 */
	class Error : public std::exception
	{
	public:
		Error(const std::string& _message):
				message(_message)
		{};

		virtual ~Error(){}

		/**
		 * @brief Get text representation of error
		 *
		 * @return error text message
		 */
		const char *what() const noexcept
		{ return this->message.c_str(); }

	private:
		std::string message;
	};

} //namespace EventMonitor;

#endif // EVENT_MONITOR_SERVICE_ERROR_HPP
