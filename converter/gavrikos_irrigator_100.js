const zigbeeHerdsmanConverters = require('zigbee-herdsman-converters');
const exposes = (zigbeeHerdsmanConverters.hasOwnProperty('exposes'))?zigbeeHerdsmanConverters.exposes:require("zigbee-herdsman-converters/lib/exposes");
const ea = exposes.access;
const e = exposes.presets;
const fz = zigbeeHerdsmanConverters.fromZigbeeConverters || zigbeeHerdsmanConverters.fromZigbee;
const tz = zigbeeHerdsmanConverters.toZigbeeConverters || zigbeeHerdsmanConverters.toZigbee;
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const ACCESS_STATE = 0b001, ACCESS_WRITE = 0b010, ACCESS_READ = 0b100;
const ZCL_DATATYPE_UINT16 = 0x21;
const ZCL_DATATYPE_UINT32 = 0x23;
const ZCL_DATATYPE_UINT48 = 0x25;




const definition = {
    zigbeeModel: ['Irrigator 1.0'],
    model: 'Irrigator 1.0',
    vendor: 'Gavrikos',
    description: 'Irrigator 1.0',
	supports: 'humidity,genBasic', 
	fromZigbee: [fz.humidity],                                                                                       
    toZigbee: [tz.on_off],
configure: async (device, coordinatorEndpoint, logger) => {
        const first_endpoint = device.getEndpoint(1);
        await reporting.bind(first_endpoint, coordinatorEndpoint, ['genBasic', 'msRelativeHumidity', 'genOnOff']);
		await reporting.onOff(first_endpoint);
        await reporting.humidity(first_endpoint);

        },
	exposes: [
            exposes.presets.humidity(), 
            e.switch()
		]
};

module.exports = definition;
