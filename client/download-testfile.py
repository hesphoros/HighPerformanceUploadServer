import os
import time
import requests
from urllib.parse import urlparse
from concurrent.futures import ThreadPoolExecutor, as_completed

# 保存目录
SAVE_DIR = "downloads"
os.makedirs(SAVE_DIR, exist_ok=True)

# 请求头模拟浏览器
HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"
}

# 最大重试次数
MAX_RETRIES = 3
# 所有文件链接（包含你提供的全部链接）
urls = [
    # 原始链接（前两次）
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_GIF_1MB.gif",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_GIF_500kB.gif",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2020/03/file_example_SVG_30kB.svg",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2020/03/file_example_WEBP_50kB.webp",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2020/03/file_example_WEBP_250kB.webp",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2020/03/file_example_WEBP_500kB.webp",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_favicon.ico",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/11/file_example_MP3_700KB.mp3",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_PNG_3MB.png",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_PNG_2100kB.png",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_JPG_500kB.jpg",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/zip_9MB.zip",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/04/file_example_MP4_1280_10MG.mp4",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file-example_PDF_1MB.pdf",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2019/09/file-sample_100kB.rtf",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2019/09/file-sample_500kB.rtf",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/04/file_example_MP4_640_3MG.mp4",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/11/file_example_MP3_2MG.mp3",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/file_example_CSV_5000.csv",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_PNG_1MB.png",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_JPG_1MB.jpg",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file-example_PDF_500_kB.pdf",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/file-sample_500kB.doc",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_JPG_100kB.jpg",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/index.html",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_JPG_100kB.jpg",

    # 第三次新增链接（使用多线程）
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/10/file_example_PNG_500kB.png",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/file-sample_1MB.docx",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/file_example_XML_24kb.xml",
    "https://file-examples.com/storage/feaa6a7f0468517af9bc02d/2017/02/file-sample_1MB.doc",
]

# 下载函数，支持重试
def download_file(url):
    filename = os.path.basename(urlparse(url).path)
    filepath = os.path.join(SAVE_DIR, filename)

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            print(f"⬇ 正在下载 [{attempt}/{MAX_RETRIES}]: {filename}")
            response = requests.get(url, stream=True, headers=HEADERS, timeout=30)
            response.raise_for_status()
            with open(filepath, "wb") as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)
            print(f"✅ 下载完成: {filename}")
            return  # 成功后直接返回
        except Exception as e:
            print(f"⚠️ 第 {attempt} 次下载失败: {filename}，错误: {e}")
            time.sleep(2)  # 等待后重试

    print(f"❌ 最终失败: {filename}")

# 多线程执行
def main():
    MAX_THREADS = min(5, len(urls))  # 避免同时请求太多
    with ThreadPoolExecutor(max_workers=MAX_THREADS) as executor:
        futures = [executor.submit(download_file, url) for url in urls]
        for future in as_completed(futures):
            pass

if __name__ == "__main__":
    main()