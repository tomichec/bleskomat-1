/*
	Copyright (C) 2020 Samotari (Charles Hill, Carlos Garcia Ortiz)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <string>
#include "config.h"
#include "display.h"
#include "logger.h"
#include "lnurl.h"
#include "modules.h"
#include "sdcard.h"

void setup() {
	Serial.begin(115200);
	logger::enable();
	if ( sdcard::open() < 0 ) {
	    printf("SD card failed to open, setting default config.\n");
	    // config::setDefault();
	} else {
	    printf("Setting config from the SD card.\n");
	    config::setConfig(sdcard::getIFStream());
	}
	sdcard::close();
	logger::write("Config OK");
	config::init();

	display::init();
	display::updateAmount(0.00, config::getFiatCurrency());
	logger::write("Display OK");
	coinAcceptor::init();
	coinAcceptor::setFiatCurrency(config::getFiatCurrency());
	logger::write("Coin Reader OK");
	logger::write("Setup OK");
}

const unsigned long bootTime = millis();// milliseconds
const unsigned long minWaitAfterBootTime = 2000;// milliseconds
const unsigned long minWaitTimeSinceInsertedFiat = 15000;// milliseconds
const unsigned long maxTimeDisplayQrCode = 120000;// milliseconds

void loop() {
  // sdcard::debug();

	if (millis() - bootTime >= minWaitAfterBootTime) {
		// Minimum time has passed since boot.
		// Start performing checks.
		coinAcceptor::loop();
		if (display::getTimeSinceRenderedQRCode() >= maxTimeDisplayQrCode) {
			// Automatically clear the QR code from the screen after some time has passed.
			display::clearQRCode();
		} else if (coinAcceptor::coinInserted() && display::hasRenderedQRCode()) {
			// Clear the QR code when new coins are inserted.
			display::clearQRCode();
		}
		float accumulatedValue = coinAcceptor::getAccumulatedValue();
		if (
			accumulatedValue > 0 &&
			coinAcceptor::getTimeSinceLastInserted() >= minWaitTimeSinceInsertedFiat
		) {
			printf("accumulatedValue: %f\n", accumulatedValue);
			// The minimum required wait time between coins has passed.
			// Create a withdraw request and render it as a QR code.
			std::string req = lnurl::create_signed_withdraw_request(
				accumulatedValue,
				config::getFiatCurrency(),
				config::getApiKeyId(),
				config::getApiKeySecret(),
				config::getCallbackUrl()
			);
			display::renderQRCode("lightning:" + req);
			coinAcceptor::reset();
		}
		if (!display::hasRenderedQRCode() && display::getRenderedAmount() != accumulatedValue) {
		    display::updateAmount(accumulatedValue, config::getFiatCurrency());
		}
	}
}
