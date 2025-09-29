@echo off
REM === 설정 부분 ===
setlocal

REM gRPC / Protobuf 설치 경로 지정 (수정 필요)
set PROTOC_BIN=protoc.exe
set GRPC_PLUGIN=grpc_cpp_plugin.exe

REM proto 파일이 있는 폴더 (수정 필요)
set PROTO_SRC=protos

REM 출력 폴더
set OUT_DIR=generated

REM === 실제 실행 ===
echo Generating protobuf and gRPC code...
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

for %%f in (%PROTO_SRC%\*.proto) do (
    echo Processing %%~nxf
    "%PROTOC_BIN%" ^
        -I../Source/ThirdParty/gRPC/include ^
        --proto_path=%PROTO_SRC% ^
        --cpp_out=%OUT_DIR% ^
        --grpc_out=%OUT_DIR% ^
        --plugin=protoc-gen-grpc="%GRPC_PLUGIN%" ^
        "%%f"
)

echo Done. Generated files are in %OUT_DIR%.
endlocal
pause