#include "server/protocol.h"

static std::string __ServerCommandToString(vuprs::ServerCommand cmd)
{
	switch (cmd)
	{
	case vuprs::ServerCommand::SERVER_CMD__RESET:
	{
		return SERVER_CMD__RESET__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__REDIRECT:
	{
		return SERVER_CMD__REDIRECT__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
	{
		return SERVER_CMD__CHANGE_BEAMFORMER__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM:
	{
		return SERVER_CMD__CHANGE_ALG_PARAM__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__STOP:
	{
		return SERVER_CMD__STOP__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__START:
	{
		return SERVER_CMD__START__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA:
	{
		return SERVER_CMD__GET_NEW_DATA__STR;
	}
	case vuprs::ServerCommand::SERVER_CMD__ACK:
	{
		return SERVER_CMD__ACK__STR;
	}
	default:
	{
		return SERVER_CMD__INVALID__STR;
	}
	}
}

bool vuprs::PROTOCOL_ParseCommandFromMessage(const std::string &message, vuprs::ServerCommandInformation *cmd)
{
	if (cmd == nullptr)
		return false;
	if (message.empty())
		return false;

	cmd->config.ResetMask(false);

	try
	{
		nlohmann::json root = nlohmann::json::parse(message);

		if (!root.contains("cmd") || !root["cmd"].is_string())
			return false;

		std::string command = root["cmd"].get<std::string>();

		if (command == SERVER_CMD__RESET__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__RESET;
			return true;
		}
		else if (command == SERVER_CMD__ACK__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__ACK;
			return true;
		}
		else if (command == SERVER_CMD__CHANGE_BEAMFORMER__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("beamformer") || !root["params"]["beamformer"].is_string())
				return false;

			const std::string beamformer_name = root["params"]["beamformer"].get<std::string>();
			if (beamformer_name != "mvdr" && beamformer_name != "cbf" && beamformer_name != "dcrcb")
				return false;

			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER;
			cmd->beamformer_name = beamformer_name;
			return true;
		}
		else if (command == SERVER_CMD__REDIRECT__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("alt") || !root["params"].contains("az"))
				return false;
			double alt = 0.0;
			double az = 0.0;
			auto params = root["params"];
			vuprs::__JsonStringParseFLOAT<double>(&alt, params, "alt", true);
			vuprs::__JsonStringParseFLOAT<double>(&az, params, "az", true);
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__REDIRECT;
			cmd->config.bf_target__alt = alt;
			cmd->config.bf_target__az = az;
			cmd->config.mask.m_bf_target__alt = true;
			cmd->config.mask.m_bf_target__az = true;
			return true;
		}
		else if (command == SERVER_CMD__SCAN_FOR_POSITION_POWER__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("points") || !root["params"]["points"].is_string())
				return false;
			if (!root["params"].contains("alt_min") || !root["params"]["alt_min"].is_string())
				return false;
			int points_in_hemisphere = 0;
			double alt_min = 0.0;
			auto params = root["params"];
			vuprs::__JsonStringParseINT<int>(&points_in_hemisphere, params, "points", true);
			vuprs::__JsonStringParseFLOAT<double>(&alt_min, params, "alt_min", true);
			if (alt_min < 0.0 || alt_min > 90.0)
				return false;
			if (points_in_hemisphere <= 0)
				return false;
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__SCAN_FOR_POSITION_POWER;
			cmd->scan_config.points_in_hemisphere = points_in_hemisphere;
			cmd->scan_config.alt_min = alt_min;
			return true;
		}
		else if (command == SERVER_CMD__CHANGE_ALG_PARAM__STR)
		{
			/*
				[HEADER]
				{
					"cmd": "change_algorithm_parameters",
					"params": {
						"fs": "40000.0",
						"wave_velocity": "346.0",
						"lower_frequency": "100.0",
						"upper_frequency": "4000.0",
						"snapshot_window_size": "100",
						"covariance_average_index": "0.8"
					}
				}
				[TAILER]
			*/
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM;
			auto params = root["params"];
			if (params.contains("fs"))
			{
				double fs = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&fs, params, "fs", true);
				cmd->config.fs = fs;
				cmd->config.mask.m_fs = true;
			}
			if (params.contains("wave_velocity"))
			{
				double wave_velocity = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&wave_velocity, params, "wave_velocity", true);
				cmd->config.bf_wave_velocity = wave_velocity;
				cmd->config.mask.m_bf_waveVelocity = true;
			}
			if (params.contains("lower_frequency"))
			{
				double lowerFreq = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&lowerFreq, params, "lower_frequency", true);
				cmd->config.bf_freq__lower = lowerFreq;
				cmd->config.mask.m_bf_freq__lower = true;
			}
			if (params.contains("upper_frequency"))
			{
				double upperFreq = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&upperFreq, params, "upper_frequency", true);
				cmd->config.bf_freq__upper = upperFreq;
				cmd->config.mask.m_bf_freq__upper = true;
			}
			if (params.contains("snapshot_window_size"))
			{
				int snapshotWindowSize = 0;
				vuprs::__JsonStringParseFLOAT<int>(&snapshotWindowSize, params, "snapshot_window_size", true);
				cmd->config.bf_cov_snapshots_window_size = snapshotWindowSize;
				cmd->config.mask.m_bf_cov_snapshots_window_size = true;
			}
			if (params.contains("covariance_average_index"))
			{
				double covarianceAverageIndex = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&covarianceAverageIndex, params, "covariance_average_index", true);
				cmd->config.bf_cov_freq_average_index = covarianceAverageIndex;
				cmd->config.mask.m_bf_cov_freq_average_index = true;
			}
			return true;
		}
		else if (command == SERVER_CMD__GET_ALG_PARAM__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM;
			return true;
		}
		else if (command == SERVER_CMD__START__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__START;
			return true;
		}
		else if (command == SERVER_CMD__STOP__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__STOP;
			return true;
		}
		else if (command == SERVER_CMD__GET_NEW_DATA__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA;
			return true;
		}
	}
	catch (...)
	{
		return false;
	}

	return false;
}

std::string vuprs::PROTOCOL_MakeServerOperationResponse(const vuprs::ServerCommandInformation &cmd, const std::string &info, bool operation_status)
{
	nlohmann::json response;
	response["response_cmd"] = __ServerCommandToString(cmd.cmd);
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	return response.dump();
}

std::string vuprs::PROTOCOL_MakeServerResultDataResponse(const std::string &info,
														 int inference_identity,
														 bool operation_status)
{
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__GET_NEW_DATA__STR;
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	response["params"]["data_format"] = "uint32_t";
	response["params"]["quantization"] = "q31";
	response["params"]["inference_identity"] = std::to_string(inference_identity);
	return response.dump();
}

std::string vuprs::PROTOCOL_MakeServerParameterResponse(const vuprs::HybridBeamformerConfig &config)
{
	/*
		{
			"response_cmd": "read_algorithm_parameters",
			"operation-status": "done",
			"params": {
				"fs": "40000.0",
				"wave_velocity": "346.0",
				"lower_frequency": "100.0",
				"upper_frequency": "4000.0",
				"snapshot_window_size": "100",
				"covariance_average_index": "0.8"
			}
		}
	*/
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__GET_ALG_PARAM__STR;
	response["operation-status"] = "done";
	response["params"]["fs"] = std::to_string(config.fs);
	response["params"]["wave_velocity"] = std::to_string(config.bf_wave_velocity);
	response["params"]["lower_frequency"] = std::to_string(config.bf_freq__lower);
	response["params"]["upper_frequency"] = std::to_string(config.bf_freq__upper);
	response["params"]["snapshot_window_size"] = std::to_string(config.bf_cov_snapshots_window_size);
	response["params"]["covariance_average_index"] = std::to_string(config.bf_cov_freq_average_index);
	return response.dump();
}

std::string vuprs::PROTOCOL_MakeServerScanningResponse(const vuprs::ScanningConfig &scan_config,
													   double min_scan_power_dB, double max_scan_power_dB,
													   const std::string &info, bool operation_status)
{
	/*
		{
			"response_cmd": "power_scan",
			"operation-status": "done",
			"params": {
				"points": "70",
				"min_alt": "15.0",
				"max_power": "12.04046",
				"min_power": "-15.00231",
				"data_format": "uint16_t"
			}
		}
	*/
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__SCAN_FOR_POSITION_POWER__STR;
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	response["params"]["points"] = std::to_string(scan_config.points_in_hemisphere);
	response["params"]["min_alt"] = std::to_string(scan_config.alt_min);
	response["params"]["max_power"] = std::to_string(max_scan_power_dB);
	response["params"]["min_power"] = std::to_string(min_scan_power_dB);
	response["params"]["data_format"] = "uint16_t";
	response["params"]["quantization"] = "q15";
	return response.dump();
}
