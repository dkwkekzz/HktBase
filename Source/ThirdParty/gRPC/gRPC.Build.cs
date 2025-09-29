using UnrealBuildTool;
using System.IO;

public class gRPC : ModuleRules
{
    public gRPC(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string ThirdPartyPath = Path.Combine(ModuleDirectory);
        string LibPath = Path.Combine(ThirdPartyPath, "lib");
        string BinPath = Path.Combine(ThirdPartyPath, "bin");

        PublicSystemIncludePaths.AddRange(
            new string[] {
                Path.Combine(ThirdPartyPath, "include"),
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // 링크할 라이브러리 목록 (Release 빌드 기준)
            string[] gRPCLibraries = new string[]
            {
                // Abseil
                "absl_base.lib", "absl_city.lib", "absl_civil_time.lib", "absl_cord.lib", "absl_cordz_functions.lib",
                "absl_cordz_handle.lib", "absl_cordz_info.lib", "absl_cordz_sample_token.lib", "absl_cord_internal.lib",
                "absl_crc32c.lib", "absl_crc_cord_state.lib", "absl_crc_cpu_detect.lib", "absl_crc_internal.lib",
                "absl_debugging_internal.lib", "absl_decode_rust_punycode.lib", "absl_demangle_internal.lib",
                "absl_demangle_rust.lib", "absl_die_if_null.lib", "absl_examine_stack.lib", "absl_exponential_biased.lib",
                "absl_failure_signal_handler.lib", "absl_flags_commandlineflag.lib", "absl_flags_commandlineflag_internal.lib",
                "absl_flags_config.lib", "absl_flags_internal.lib", "absl_flags_marshalling.lib", "absl_flags_parse.lib",
                "absl_flags_private_handle_accessor.lib", "absl_flags_program_name.lib", "absl_flags_reflection.lib",
                "absl_flags_usage.lib", "absl_flags_usage_internal.lib", "absl_graphcycles_internal.lib",
                "absl_hash.lib", "absl_hashtablez_sampler.lib", "absl_int128.lib", "absl_kernel_timeout_internal.lib",
                "absl_leak_check.lib", "absl_log_flags.lib", "absl_log_globals.lib", "absl_log_initialize.lib",
                "absl_log_internal_check_op.lib", "absl_log_internal_conditions.lib", "absl_log_internal_fnmatch.lib",
                "absl_log_internal_format.lib", "absl_log_internal_globals.lib", "absl_log_internal_log_sink_set.lib",
                "absl_log_internal_message.lib", "absl_log_internal_nullguard.lib", "absl_log_internal_proto.lib",
                "absl_log_internal_structured_proto.lib", "absl_log_severity.lib", "absl_log_sink.lib",
                "absl_low_level_hash.lib", "absl_malloc_internal.lib", "absl_periodic_sampler.lib", "absl_poison.lib",
                "absl_random_distributions.lib", "absl_random_internal_distribution_test_util.lib",
                "absl_random_internal_entropy_pool.lib", "absl_random_internal_platform.lib",
                "absl_random_internal_randen.lib", "absl_random_internal_randen_hwaes.lib",
                "absl_random_internal_randen_hwaes_impl.lib", "absl_random_internal_randen_slow.lib",
                "absl_random_internal_seed_material.lib", "absl_random_seed_gen_exception.lib",
                "absl_random_seed_sequences.lib", "absl_raw_hash_set.lib", "absl_raw_logging_internal.lib",
                "absl_scoped_set_env.lib", "absl_spinlock_wait.lib", "absl_stacktrace.lib", "absl_status.lib",
                "absl_statusor.lib", "absl_strerror.lib", "absl_strings.lib", "absl_strings_internal.lib",
                "absl_string_view.lib", "absl_str_format_internal.lib", "absl_symbolize.lib",
                "absl_synchronization.lib", "absl_throw_delegate.lib", "absl_time.lib", "absl_time_zone.lib",
                "absl_tracing_internal.lib", "absl_utf8_for_code_point.lib", "absl_vlog_config_internal.lib",

                // gRPC Dependencies
                "address_sorting.lib", "cares.lib", "crypto.lib", "re2.lib", "ssl.lib",
                
                // Protobuf & UPB
                "libprotobuf-lite.lib", "libprotobuf.lib", "libprotoc.lib", "libupb.lib", "upb_base_lib.lib",
                "upb_hash_lib.lib", "upb_json_lib.lib", "upb_lex_lib.lib", "upb_mem_lib.lib", "upb_message_lib.lib",
                "upb_mini_descriptor_lib.lib", "upb_mini_table_lib.lib", "upb_reflection_lib.lib",
                "upb_textformat_lib.lib", "upb_wire_lib.lib",

                // UTF8
                "libutf8_range.lib", "libutf8_validity.lib", "utf8_range_lib.lib",
                
                // Zlib
                "zlibstatic.lib",
                
                // gRPC Core
                "gpr.lib", "grpc.lib", "grpc_unsecure.lib", "grpc++.lib", "grpc++_alts.lib",
                "grpc++_error_details.lib", "grpc++_reflection.lib", "grpc++_unsecure.lib",
                "grpcpp_channelz.lib", "grpc_authorization_provider.lib", "grpc_plugin_support.lib"
            };

            foreach (string lib in gRPCLibraries)
            {
                PublicAdditionalLibraries.Add(Path.Combine(LibPath, lib));
            }

            // DLL 복사 (런타임 의존성)
            string[] gRPCDlls = Directory.GetFiles(BinPath, "*.dll");
            foreach (string dllPath in gRPCDlls)
            {
                RuntimeDependencies.Add(dllPath);
            }
        }
    }
}
