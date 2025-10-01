#!/usr/bin/env python3
"""
TOML配置文件验证和生成工具
用于验证upload_client.toml配置文件的格式正确性

使用方法:
python toml_validator.py validate config/upload_client.toml
python toml_validator.py generate > config/default_config.toml
"""

import sys
import argparse
from pathlib import Path

try:
    import tomllib  # Python 3.11+
except ImportError:
    try:
        import tomli as tomllib  # Python < 3.11
    except ImportError:
        print("请安装tomli库: pip install tomli")
        sys.exit(1)

def validate_toml_file(file_path):
    """验证TOML文件格式和结构"""
    try:
        with open(file_path, 'rb') as f:
            data = tomllib.load(f)
        
        print(f"✅ TOML文件格式正确: {file_path}")
        
        # 验证必需的配置节
        required_sections = ['upload', 'ui', 'network']
        missing_sections = []
        
        for section in required_sections:
            if section not in data:
                missing_sections.append(section)
        
        if missing_sections:
            print(f"⚠️  缺少配置节: {', '.join(missing_sections)}")
        else:
            print("✅ 所有必需的配置节都存在")
        
        # 验证upload节的关键配置
        if 'upload' in data:
            upload = data['upload']
            
            # 检查关键配置项
            key_configs = {
                'server_host': str,
                'server_port': int,
                'max_concurrent_uploads': int,
                'chunk_size': int,
                'compression_algorithm': str,
                'checksum_algorithm': str
            }
            
            for key, expected_type in key_configs.items():
                if key in upload:
                    value = upload[key]
                    if not isinstance(value, expected_type):
                        print(f"❌ {key} 类型错误: 期望 {expected_type.__name__}, 得到 {type(value).__name__}")
                    else:
                        print(f"✅ {key}: {value}")
                else:
                    print(f"⚠️  缺少配置项: {key}")
            
            # 验证枚举值
            if 'compression_algorithm' in upload:
                valid_compression = ['none', 'gzip', 'deflate', 'lz4', 'brotli']
                if upload['compression_algorithm'] not in valid_compression:
                    print(f"❌ 无效的压缩算法: {upload['compression_algorithm']}")
                    print(f"   有效值: {', '.join(valid_compression)}")
            
            if 'checksum_algorithm' in upload:
                valid_checksum = ['none', 'md5', 'sha1', 'sha256', 'crc32']
                if upload['checksum_algorithm'] not in valid_checksum:
                    print(f"❌ 无效的校验算法: {upload['checksum_algorithm']}")
                    print(f"   有效值: {', '.join(valid_checksum)}")
        
        return True
        
    except tomllib.TOMLDecodeError as e:
        print(f"❌ TOML语法错误: {e}")
        return False
    except FileNotFoundError:
        print(f"❌ 文件不存在: {file_path}")
        return False
    except Exception as e:
        print(f"❌ 验证过程中出错: {e}")
        return False

def generate_default_config():
    """生成默认的TOML配置文件内容"""
    config = '''# 高性能文件上传客户端 - 默认配置
# 生成时间: 2025-09-30

[upload]
server_host = "localhost"
server_port = 8080
upload_protocol = "HTTP"
max_concurrent_uploads = 4
chunk_size = 1048576
timeout_seconds = 30
retry_count = 3
retry_delay_ms = 1000
max_upload_speed = 0
max_file_size = 0
enable_resume = true
enable_compression = false
compression_algorithm = "gzip"
enable_checksum = true
checksum_algorithm = "md5"
overwrite = false
enable_multipart = true
enable_progress = true
target_dir = "/uploads"
exclude_patterns = ["*.tmp", "*.log", ".*"]
use_ssl = false
cert_file = ""
private_key_file = ""
ca_file = ""
verify_server = true
auth_token = ""
log_level = "INFO"
log_file_path = "logs/client.log"
enable_detailed_log = false
client_version = "1.0.0"
user_agent = "HighPerformanceUploadClient/1.0.0"

[ui]
show_progress_details = true
show_speed_info = true
auto_start_upload = false
minimize_to_tray = true
show_notifications = true
language = "zh_CN"
theme = "default"
window_width = 800
window_height = 600
window_maximized = false
show_file_size = true
show_file_type = true
show_upload_time = true
show_file_status = true

[network]
connect_timeout_ms = 5000
read_timeout_ms = 30000
write_timeout_ms = 30000
buffer_size = 8192
max_connections = 10
enable_keep_alive = true
keep_alive_interval_ms = 30000
enable_proxy = false
proxy_host = ""
proxy_port = 8080
proxy_user = ""
proxy_password = ""
'''
    return config.strip()

def main():
    parser = argparse.ArgumentParser(description='TOML配置文件验证和生成工具')
    subparsers = parser.add_subparsers(dest='command', help='可用命令')
    
    # 验证命令
    validate_parser = subparsers.add_parser('validate', help='验证TOML文件')
    validate_parser.add_argument('file', help='要验证的TOML文件路径')
    
    # 生成命令
    generate_parser = subparsers.add_parser('generate', help='生成默认配置')
    generate_parser.add_argument('--output', '-o', help='输出文件路径')
    
    args = parser.parse_args()
    
    if args.command == 'validate':
        success = validate_toml_file(args.file)
        sys.exit(0 if success else 1)
    
    elif args.command == 'generate':
        config = generate_default_config()
        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(config)
            print(f"✅ 默认配置已生成: {args.output}")
        else:
            print(config)
    
    else:
        parser.print_help()

if __name__ == '__main__':
    main()