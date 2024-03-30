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


const tz_local = {
    se_metering: {
        key: ['measurement_period'],
        convertSet: async (entity, key, value, meta) => {
            value *= 1;
            const payloads = {
                measurement_period: ['seMetering', {0XF002: {value, type: ZCL_DATATYPE_UINT16}}],
            };
            await entity.write(payloads[key][0], payloads[key][1]);
            return {
                state: {[key]: value},
            };
        },
        convertGet: async (entity, key, meta) => {
            const payloads = {
                measurement_period: ['seMetering', 0XF002],
            };
            await entity.read(payloads[key][0], [payloads[key][1]]);
        },
    },
};


const definition = {
    zigbeeModel: ['Irrigator 1.0'],
    model: 'Irrigator 1.0',
    vendor: 'Gavrikos',
    description: 'Irrigator 1.0',
	supports: 'temperature,genBasic', 
	fromZigbee: [fz.temperature],                                                                                       
    toZigbee: [tz.on_off, tz_local.se_metering],
configure: async (device, coordinatorEndpoint, logger) => {
        const first_endpoint = device.getEndpoint(1);
        await reporting.bind(first_endpoint, coordinatorEndpoint, ['genBasic', 'msTemperatureMeasurement', 'genOnOff']);
		await reporting.onOff(first_endpoint);
        await reporting.temperature(first_endpoint);

        },
	exposes: [exposes.numeric('temperature', ACCESS_STATE).withUnit('�C').withDescription('Measured temperature value'), e.switch(),
		e.numeric('measurement_period', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withDescription('Measurement Period').withValueMin(0).withValueMax(600)
		]
};

module.exports = definition;
