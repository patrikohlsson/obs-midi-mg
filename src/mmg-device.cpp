/*
obs-midi-mg
Copyright (C) 2022-2024 nhielost <nhielost@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "mmg-device.h"
#include "mmg-config.h"
#include "mmg-binding.h"
#include "mmg-message.h"

using namespace MMGUtils;

// MMGDevice
MMGDevice::MMGDevice(MMGDeviceManager *parent, const QJsonObject &json_obj) : MMGMIDIPort(parent, json_obj)
{
	update(json_obj);
}

void MMGDevice::json(QJsonObject &device_obj) const
{
	device_obj["name"] = objectName();
	device_obj["active"] = (int)_active;
	if (!_thru.isEmpty()) device_obj["thru"] = _thru;
}

void MMGDevice::update(const QJsonObject &json_obj)
{
	// Store the active state we want to set
	uint activeState = json_obj["active"].toInt();
		
	// Apply the active states
	if (activeState & 0b01) {
		setActive(TYPE_INPUT, true);
	} else {
		setActive(TYPE_INPUT, false);
	}
	
	if (activeState & 0b10) {
		setActive(TYPE_OUTPUT, true);
	} else {
		setActive(TYPE_OUTPUT, false);
	}
	
	_thru = json_obj["thru"].toString();
}

bool MMGDevice::isActive(DeviceType type) const
{
	switch (type) {
		case TYPE_INPUT:
			return _active & 0b01;

		case TYPE_OUTPUT:
			return _active & 0b10;

		default:
			return _active > 0;
	}
}

void MMGDevice::setActive(DeviceType type, bool active)
{
	if (!editable || isActive(type) == active || !isCapable(type)) return;

	!isActive(type) ? openPort(type) : closePort(type);
	// Is the port not in the correct state?
	if (isPortOpen(type) == isActive(type)) {
		MMGInterface::promptUser("PortOpenError");
		return;
	}

	switch (type) {
		case TYPE_INPUT:
			_active ^= 0b01;
			break;

		case TYPE_OUTPUT:
			_active ^= 0b10;
			break;

		default:
			break;
	}
	
	// If we're activating a device, check for any bindings that might use this device
	// and refresh their connections
	if (active) {
		// Get all collections
		MMGConfig *cfg = config();
		if (cfg) {
			// Refresh any bindings that might use this device
			for (MMGBindingManager *collection : *cfg->collections()) {
				for (MMGBinding *binding : *collection) {
					// Check if this binding uses this device
					bool needsRefresh = false;
					
					// Check the first message's device name
					if (binding->messages()->size() > 0) {
						MMGMessage *message = binding->messages(0);
						if (message && message->deviceName() == objectName()) {
							needsRefresh = true;
						}
					}
					
					if (needsRefresh) {
						binding->refresh();
					}
				}
			}
		}
	}
}

void MMGDevice::checkCapable()
{
	uint active = _active;
	_active = 0;

	blog(LOG_INFO, "Checking device capabilities...");

	closePort(TYPE_INPUT);
	closePort(TYPE_OUTPUT);

	openPort(TYPE_INPUT);
	setCapable(TYPE_INPUT, isPortOpen(TYPE_INPUT));

	openPort(TYPE_OUTPUT);
	setCapable(TYPE_OUTPUT, isPortOpen(TYPE_OUTPUT));

	closePort(TYPE_INPUT);
	closePort(TYPE_OUTPUT);

	blog(LOG_INFO, "Device capabilities checked. Re-opening active ports...");

	setActive(TYPE_INPUT, active & 0b01);
	setActive(TYPE_OUTPUT, active & 0b10);
}
// End MMGDevice

// MMGDeviceManager
MMGDevice *MMGDeviceManager::add(const QJsonObject &json_obj)
{
	QString deviceName = json_obj["name"].toString();
	MMGDevice *current_device = find(deviceName);

	if (current_device) {
		current_device->update(json_obj);
		// After updating an existing device, refresh all bindings that use it
		refreshBindingsForDevice(deviceName);
		return current_device;
	} else {
		if (find(mmgtr("Device.Dummy"))) remove(find(mmgtr("Device.Dummy")));
		
		// Create new device
		MMGDevice *newDevice = new MMGDevice(this, json_obj);
		
		// Add the device to our manager
		MMGManager::add(newDevice);
		
		// Update all message references that might be using this device name
		updateDeviceReferences(deviceName, newDevice);
		
		// After adding a new device, refresh all bindings that would use it
		refreshBindingsForDevice(deviceName);
		
		return newDevice;
	}
}

void MMGDeviceManager::updateDeviceReferences(const QString &deviceName, MMGDevice *newDevice)
{
	// Get access to the global config to iterate through all collections and bindings
	MMGConfig *cfg = config();
	if (!cfg) return;
	
	// Iterate through all collections
	for (MMGBindingManager *collection : *cfg->collections()) {
		// Iterate through all bindings in this collection
		for (MMGBinding *binding : *collection) {
			// Iterate through all messages in this binding
			for (MMGMessage *message : *binding->messages()) {
				// If the message has an empty device or the device name matches,
				// update to point to the new device
				if (!message->device() || (message->device() && message->device()->objectName() == deviceName)) {
					message->setDevice(newDevice);
				}
			}
		}
	}
}

void MMGDeviceManager::refreshBindingsForDevice(const QString &deviceName)
{
	// Get access to the global config to iterate through all collections and bindings
	MMGConfig *cfg = config();
	if (!cfg) return;
	
	// Iterate through all collections
	for (MMGBindingManager *collection : *cfg->collections()) {
		// Iterate through all bindings in this collection
		for (MMGBinding *binding : *collection) {
			// Check if this binding uses this device
			bool needsRefresh = false;
			
			// Check all messages in the binding
			for (MMGMessage *message : *binding->messages()) {
				if (message->deviceName() == deviceName) {
					needsRefresh = true;
					break;
				}
			}
			
			if (needsRefresh) {
				binding->refresh();
			}
		}
	}
}

MMGDevice *MMGDeviceManager::add(const QString &name)
{
	QJsonObject json_obj;
	json_obj["name"] = name;
	return add(json_obj);
}

const QStringList MMGDeviceManager::capableDevices(DeviceType type) const
{
	QStringList devices;
	for (MMGDevice *device : _list) {
		if (device->isCapable(type)) devices += device->objectName();
	}
	return devices;
}
// End MMGDeviceManager
