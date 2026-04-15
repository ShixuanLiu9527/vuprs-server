#include "protocol.h"

static std::string __ServerCommandToString(vuprs::ServerCommand cmd)
{
	switch (cmd)
	{
		case vuprs::ServerCommand::SERVER_CMD__RESET:
		{
			return "reset";
		}
		case vuprs::ServerCommand::SERVER_CMD__REDIRECT:
		{
			return "redirect";
		}
		case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
		{
			return "change_beam_former";
		}
		case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM:
		{
			return "change_algorithm_parameters";
		}
		case vuprs::ServerCommand::SERVER_CMD__STOP:
		{
			return "stop";
		}
		case vuprs::ServerCommand::SERVER_CMD__START:
		{
			return "start";
		}
		case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA:
		{
			return "get_data";
		}
		case vuprs::ServerCommand::SERVER_CMD__ACK:
		{
			return "ack";
		}
		default:
		{
			return "invalid";
		}
	}
}

bool vuprs::PROTOCOL_ParseCommandFromMessage(const std::string &message, vuprs::ServerCommandInformation *cmd)
{
	if (cmd == nullptr) return false;
	if (message.empty()) return false;

	cmd->configMask.Reset();

	try
	{
		nlohmann::json root = nlohmann::json::parse(message);

		if (!root.contains("cmd") || !root["cmd"].is_string()) return false;

		std::string command = root["cmd"].get<std::string>();

		if (command == "reset")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__RESET;
			return true;
		}
		else if (command == "ack")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__ACK;
			return true;
		}
		else if (command == "change_beam_former")
		{
			if (!root.contains("params") || !root["params"].is_object()) return false;
			if (!root["params"].contains("beamformer") || !root["params"]["beamformer"].is_string()) return false;

			const std::string beamformerName = root["params"]["beamformer"].get<std::string>();
			if (beamformerName != "mvdr" && beamformerName != "cbf" && beamformerName != "dcrcb") return false;

			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER;
			cmd->beamformer_name = beamformerName;
			return true;
		}
		else if (command == "redirect")
		{
			if (!root.contains("params") || !root["params"].is_object()) return false;
			if (!root["params"].contains("alt") || !root["params"].contains("az")) return false;

			double alt = 0.0;
			double az = 0.0;

			auto params = root["params"];

			vuprs::__JsonStringParseFLOAT<double>(&alt, params, "alt", true);
			vuprs::__JsonStringParseFLOAT<double>(&az, params, "az", true);

			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__REDIRECT;

			cmd->config.bf_target__alt = alt;
			cmd->config.bf_target__az = az;
			
			cmd->configMask.m_bf_target__alt = true;
			cmd->configMask.m_bf_target__az = true;

			return true;
		}
		else if (command == "change_algorithm_parameters")
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
				cmd->configMask.m_fs = true;
			}
			if (params.contains("wave_velocity"))
			{
				double waveVelocity = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&waveVelocity, params, "wave_velocity", true);
				cmd->config.bf_waveVelocity = waveVelocity;
				cmd->configMask.m_bf_waveVelocity = true;
			}
			if (params.contains("lower_frequency"))
			{
				double lowerFreq = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&lowerFreq, params, "lower_frequency", true);
				cmd->config.bf_freq__lower = lowerFreq;
				cmd->configMask.m_bf_freq__lower = true;
			}
			if (params.contains("upper_frequency"))
			{
				double upperFreq = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&upperFreq, params, "upper_frequency", true);
				cmd->config.bf_freq__upper = upperFreq;
				cmd->configMask.m_bf_freq__upper = true;
			}
			if (params.contains("snapshot_window_size"))
			{
				int snapshotWindowSize = 0;
				vuprs::__JsonStringParseFLOAT<int>(&snapshotWindowSize, params, "snapshot_window_size", true);
				cmd->config.bf_cov_snapshotsWindowSize = snapshotWindowSize;
				cmd->configMask.m_bf_cov_snapshotsWindowSize = true;
			}
			if (params.contains("covariance_average_index"))
			{
				double covarianceAverageIndex = 0.0;
				vuprs::__JsonStringParseFLOAT<double>(&covarianceAverageIndex, params, "covariance_average_index", true);
				cmd->config.bf_cov_freqAverageIndex = covarianceAverageIndex;
				cmd->configMask.m_bf_cov_freqAverageIndex = true;
			}

			return true;
		}
		else if (command == "read_algorithm_parameters")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM;
			return true;
		}
		else if (command == "start")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__START;
			return true;
		}
		else if (command == "stop")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__STOP;
			return true;
		}
		else if (command == "get_data")
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

std::string vuprs::PROTOCOL_MakeServerOperationResponse(const vuprs::ServerCommandInformation &cmd, const std::string &info, bool operationStatus)
{
	nlohmann::json response;
	response["response_cmd"] = __ServerCommandToString(cmd.cmd);
	response["operation_status"] = operationStatus ? "done" : "failed";
	response["info"] = info;
	return response.dump();
}

std::string vuprs::PROTOCOL_MakeServerParameterResponse(const vuprs::ARM_FPGA_BF_Config &config)
{
	/*
		{
		    "response_cmd": "read_algorithm_parameters",
		    "operation_status": "done",
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
	response["response_cmd"] = "read_algorithm_parameters";
	response["operation_status"] = "done";
	response["params"]["fs"] = std::to_string(config.fs);
	response["params"]["wave_velocity"] = std::to_string(config.bf_waveVelocity);
	response["params"]["lower_frequency"] = std::to_string(config.bf_freq__lower);
	response["params"]["upper_frequency"] = std::to_string(config.bf_freq__upper);
	response["params"]["snapshot_window_size"] = std::to_string(config.bf_cov_snapshotsWindowSize);
	response["params"]["covariance_average_index"] = std::to_string(config.bf_cov_freqAverageIndex);
	return response.dump();
}
