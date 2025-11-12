#ifndef MQTTV5_TIMEKEEPER_HPP
#define MQTTV5_TIMEKEEPER_HPP
/**
 * @file TimeKeeper.hpp
 *
 * This module declares the TimeKeeper interface.
 *
 * © 2025 by Hatem Nabli
 */

#include <memory>

namespace MqttV5
{
    /**
     * This represents the time-keeping requirements of MqttV5::Server and MqttV5::Client.
     * To integrate MqttV5::Server and MqttV5::Client into a large program, implement this
     * interface in terms of the Client time.
     *
     */
    class TimeKeeper
    {
        // Methods
    public:
        /**
         * This method returns the current time.
         *
         * @return
         *      The current time is returned in seconds.
         */
        virtual double GetCurrentTime() = 0;
    };
}  // namespace MqttV5

#endif /* MQTTV5_TIMEKEEPER_HPP */
