// s3curl.go - AWS Signature V4 命令行工具，兼容 S3 和 MinIO
package main

import (
	"bytes"
	"crypto/hmac"
	"crypto/md5"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

// ==================== 配置结构 ====================

type Config struct {
	Profiles map[string]Profile `json:"profiles"`
}

type Profile struct {
	ID  string `json:"id"`
	Key string `json:"key"`
}

// ==================== 命令行参数 ====================

var (
	// 全局选项
	id                  string
	key                 string
	region              string
	contentType         string
	acl                 string
	contentMd5          string
	calculateContentMd5 bool
	debug               bool
	servicePath         string
	endpoints           string

	// 操作选项
	putFile          string
	copySrc          string
	copySrcRange     string
	postFile         string // --post 的文件路径，空字符串表示空 POST
	deleteFlag       bool
	createBucketFlag bool
	headFlag         bool
	helpFlag         bool
)

func init() {
	flag.StringVar(&id, "id", "", "账号友好名或 AccessKeyId")
	flag.StringVar(&key, "key", "", "SecretAccessKey（不推荐）")
	flag.StringVar(&region, "region", "us-east-1", "AWS 区域")
	flag.StringVar(&contentType, "contentType", "", "Content-Type 头")
	flag.StringVar(&acl, "acl", "", "预定义 ACL（x-amz-acl）")
	flag.StringVar(&contentMd5, "contentMd5", "", "手动指定 Content-MD5")
	flag.BoolVar(&calculateContentMd5, "calculateContentMd5", false, "自动计算 Content-MD5")
	flag.BoolVar(&debug, "debug", false, "开启调试日志")
	flag.StringVar(&servicePath, "servicePath", "", "从资源路径中剔除的前缀（V4 未使用）")
	flag.StringVar(&endpoints, "endpoint", "", "额外 endpoint（逗号分隔，保留兼容）")

	flag.StringVar(&putFile, "put", "", "PUT 上传的本地文件")
	flag.StringVar(&copySrc, "copySrc", "", "Copy 来源（bucket/key）")
	flag.StringVar(&copySrcRange, "copySrcRange", "", "Copy 字节区间（如 0-1023）")
	flag.StringVar(&postFile, "post", "", "POST 请求的本地文件（空字符串表示空 POST）")
	flag.BoolVar(&deleteFlag, "delete", false, "执行 DELETE")
	flag.BoolVar(&createBucketFlag, "createBucket", false, "创建 bucket")
	flag.BoolVar(&headFlag, "head", false, "执行 HEAD")
	flag.BoolVar(&helpFlag, "help", false, "显示帮助")
}

// ==================== 主要逻辑 ====================

func main() {
	flag.Usage = usage
	flag.Parse()

	if helpFlag {
		usage()
		os.Exit(0)
	}

	args := flag.Args()
	if len(args) == 0 {
		fmt.Fprintf(os.Stderr, "错误：缺少 URL\n")
		usage()
		os.Exit(1)
	}
	urlStr := args[0]

	// 1. 获取凭证
	accessKey, secretKey := getCredentials(id, key)
	if accessKey == "" || secretKey == "" {
		log.Fatal("未提供有效的 AccessKeyId 和 SecretAccessKey")
	}

	// 2. 解析 URL
	parsedURL, err := url.Parse(urlStr)
	if err != nil {
		log.Fatalf("解析 URL 失败: %v", err)
	}
	host := parsedURL.Hostname()
	port := parsedURL.Port() // 可能为空
	hostHeader := host       // 强制不含端口，兼容 MinIO

	// 调试：输出 Found the url
	if debug {
		log.Printf("s3curl: endpoints: s3.amazonaws.com s3-us-west-1.amazonaws.com s3-us-west-2.amazonaws.com s3-us-gov-west-1.amazonaws.com s3-eu-west-1.amazonaws.com s3-ap-southeast-1.amazonaws.com s3-ap-northeast-1.amazonaws.com s3-sa-east-1.amazonaws.com")
		log.Printf("s3curl: Found the url: host=%s (without port), port=%s, uri=%s, query=%s;",
			host, port, parsedURL.Path, parsedURL.RawQuery)
	}

	// 3. 确定 HTTP 方法
	method := "GET"
	if putFile != "" || copySrc != "" || createBucketFlag {
		method = "PUT"
	} else if deleteFlag {
		method = "DELETE"
	} else if headFlag {
		method = "HEAD"
	} else if postFile != "" || flag.Lookup("post").Value.String() != "" {
		method = "POST"
	}
	// 检查 --post 是否显式设置（即使值为空）
	postSet := false
	for _, arg := range os.Args {
		if arg == "--post" || strings.HasPrefix(arg, "--post=") {
			postSet = true
			break
		}
	}
	if postSet {
		method = "POST"
	}

	// 4. 处理请求体与 Content-MD5 / SHA256
	var body io.Reader
	var bodyBytes []byte
	var contentMD5Val string
	var contentSHA256 string

	if putFile != "" {
		data, err := os.ReadFile(putFile)
		if err != nil {
			log.Fatalf("读取文件 %s 失败: %v", putFile, err)
		}
		bodyBytes = data
		body = bytes.NewReader(data)
	} else if createBucketFlag {
		if region != "" {
			xmlData := fmt.Sprintf(`<CreateBucketConfiguration><LocationConstraint>%s</LocationConstraint></CreateBucketConfiguration>`, region)
			bodyBytes = []byte(xmlData)
			body = bytes.NewReader(bodyBytes)
		} else {
			body = nil
			bodyBytes = []byte{}
		}
	} else if postSet {
		if postFile != "" {
			data, err := os.ReadFile(postFile)
			if err != nil {
				log.Fatalf("读取 POST 文件 %s 失败: %v", postFile, err)
			}
			bodyBytes = data
			body = bytes.NewReader(data)
		} else {
			body = nil
			bodyBytes = []byte{}
		}
	} else {
		body = nil
		bodyBytes = []byte{}
	}

	// 计算 Content-MD5
	if calculateContentMd5 {
		if len(bodyBytes) > 0 {
			hash := md5.Sum(bodyBytes)
			contentMD5Val = base64.StdEncoding.EncodeToString(hash[:])
		} else {
			emptyMD5 := md5.Sum([]byte{})
			contentMD5Val = base64.StdEncoding.EncodeToString(emptyMD5[:])
		}
	} else if contentMd5 != "" {
		contentMD5Val = contentMd5
	}

	// 计算 x-amz-content-sha256
	if len(bodyBytes) > 0 {
		hash := sha256.Sum256(bodyBytes)
		contentSHA256 = hex.EncodeToString(hash[:])
	} else {
		contentSHA256 = sha256Hex("")
	}

	// 5. 构建请求对象
	req, err := http.NewRequest(method, urlStr, body)
	if err != nil {
		log.Fatalf("构建请求失败: %v", err)
	}

	// 设置 Host 头（不含端口）
	req.Host = hostHeader

	// 设置常用头
	if contentType != "" {
		req.Header.Set("Content-Type", contentType)
	}
	if contentMD5Val != "" {
		req.Header.Set("Content-MD5", contentMD5Val)
	}
	if acl != "" {
		req.Header.Set("x-amz-acl", acl)
	}
	if copySrc != "" {
		req.Header.Set("x-amz-copy-source", copySrc)
	}
	if copySrcRange != "" {
		req.Header.Set("x-amz-copy-source-range", "bytes="+copySrcRange)
	}

	// 6. 生成 V4 签名
	now := time.Now().UTC()
	amzDate := now.Format("20060102T150405Z")
	dateStamp := now.Format("20060102")

	req.Header.Set("x-amz-date", amzDate)
	req.Header.Set("x-amz-content-sha256", contentSHA256)

	// 构建规范请求
	canonicalURI := parsedURL.Path
	if canonicalURI == "" {
		canonicalURI = "/"
	}
	canonicalQuery := parsedURL.RawQuery
	if canonicalQuery != "" {
		vals, _ := url.ParseQuery(canonicalQuery)
		keys := make([]string, 0, len(vals))
		for k := range vals {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		var parts []string
		for _, k := range keys {
			for _, v := range vals[k] {
				parts = append(parts, url.QueryEscape(k)+"="+url.QueryEscape(v))
			}
		}
		canonicalQuery = strings.Join(parts, "&")
	}

	// 构建规范头
	headers := make(map[string]string)
	for k, v := range req.Header {
		headers[strings.ToLower(k)] = strings.Join(v, ",")
	}
	headers["host"] = hostHeader

	var headerNames []string
	for k := range headers {
		headerNames = append(headerNames, k)
	}
	sort.Strings(headerNames)
	var canonicalHeaders strings.Builder
	var signedHeaders []string
	for _, k := range headerNames {
		v := headers[k]
		v = strings.TrimSpace(v)
		canonicalHeaders.WriteString(k + ":" + v + "\n")
		signedHeaders = append(signedHeaders, k)
	}
	signedHeadersStr := strings.Join(signedHeaders, ";")

	canonicalRequest := fmt.Sprintf("%s\n%s\n%s\n%s\n%s\n%s",
		method,
		canonicalURI,
		canonicalQuery,
		canonicalHeaders.String(),
		signedHeadersStr,
		contentSHA256,
	)

	if debug {
		log.Printf("s3curl: CanonicalRequest:\n%s\n", canonicalRequest)
	}

	hashedCanonicalRequest := sha256Hex(canonicalRequest)
	if debug {
		log.Printf("s3curl: HashedCanonicalRequest: %s", hashedCanonicalRequest)
	}

	credentialScope := fmt.Sprintf("%s/%s/s3/aws4_request", dateStamp, region)
	stringToSign := fmt.Sprintf("AWS4-HMAC-SHA256\n%s\n%s\n%s",
		amzDate,
		credentialScope,
		hashedCanonicalRequest,
	)

	if debug {
		log.Printf("s3curl: StringToSign:\n%s\n", stringToSign)
	}

	// 计算签名密钥
	kSecret := []byte("AWS4" + secretKey)
	kDate := hmacSHA256(kSecret, dateStamp)
	kRegion := hmacSHA256(kDate, region)
	kService := hmacSHA256(kRegion, "s3")
	kSigning := hmacSHA256(kService, "aws4_request")

	signature := hex.EncodeToString(hmacSHA256(kSigning, stringToSign))
	if debug {
		log.Printf("s3curl: Signature: %s", signature)
	}

	authHeader := fmt.Sprintf("AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s",
		accessKey,
		credentialScope,
		signedHeadersStr,
		signature,
	)
	req.Header.Set("Authorization", authHeader)

	// 7. 发送请求
	client := &http.Client{
		Timeout: 30 * time.Second,
	}

	if debug {
		// 模拟 Perl 版本的 exec curl 输出
		log.Printf("s3curl: exec curl -v -H 'Host: %s' -H 'x-amz-date: %s' -H 'Authorization: %s' -H 'x-amz-content-sha256: %s' %s %s",
			hostHeader, amzDate, authHeader, contentSHA256, method, urlStr)
	}

	resp, err := client.Do(req)
	if err != nil {
		log.Fatalf("请求失败: %v", err)
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Fatalf("读取响应体失败: %v", err)
	}

	fmt.Printf("HTTP %d %s\n", resp.StatusCode, resp.Status)
	for k, v := range resp.Header {
		fmt.Printf("%s: %s\n", k, strings.Join(v, ", "))
	}
	fmt.Println()
	if len(respBody) > 0 {
		fmt.Println(string(respBody))
	}

	if resp.StatusCode >= 400 {
		os.Exit(1)
	}
}

// ==================== 辅助函数 ====================

func getCredentials(idFlag, keyFlag string) (accessKey, secretKey string) {
	if keyFlag != "" {
		return idFlag, keyFlag
	}
	if idFlag == "" {
		log.Fatal("未指定 --id，且未提供 --key")
	}
	config, err := loadConfig()
	if err != nil {
		log.Fatalf("读取配置文件失败: %v", err)
	}
	profile, ok := config.Profiles[idFlag]
	if !ok {
		log.Fatalf("配置文件中不存在账号 '%s'", idFlag)
	}
	return profile.ID, profile.Key
}

func loadConfig() (*Config, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil, err
	}
	configPath := filepath.Join(home, ".s3curl.json")
	data, err := os.ReadFile(configPath)
	if err != nil {
		return nil, fmt.Errorf("无法读取 %s: %w", configPath, err)
	}
	var cfg Config
	if err := json.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("解析 JSON 失败: %w", err)
	}
	return &cfg, nil
}

func sha256Hex(data string) string {
	h := sha256.Sum256([]byte(data))
	return hex.EncodeToString(h[:])
}

func hmacSHA256(key []byte, data string) []byte {
	h := hmac.New(sha256.New, key)
	h.Write([]byte(data))
	return h.Sum(nil)
}

// ==================== 帮助信息 ====================

func usage() {
	fmt.Fprintf(os.Stderr, `用法: %s [选项] URL

选项:
  --id <账号名或AK>           账号友好名（配置文件中的 key）或 AccessKeyId
  --key <SecretKey>          直接指定 SecretAccessKey（不推荐）
  --region <区域>            AWS 区域（默认 us-east-1）
  --contentType <类型>        Content-Type
  --acl <权限>               预定义 ACL（如 public-read）
  --contentMd5 <base64>      手动指定 Content-MD5
  --calculateContentMd5      自动计算 Content-MD5
  --put <文件>               PUT 上传文件
  --copySrc <bucket/key>     Copy 来源
  --copySrcRange <范围>      Copy 字节范围（如 0-1023）
  --post [文件]              POST 请求（可选文件，若只写 --post 则为空 POST）
  --delete                   执行 DELETE
  --createBucket             创建 bucket（使用 --region 指定区域，默认为 us-east-1）
  --head                     执行 HEAD
  --debug                    开启调试日志
  --help                     显示帮助

示例:
  %s --id minio_root --createBucket http://127.0.0.1:9000/bucket02
  %s --id minio_root --put myfile.txt http://127.0.0.1:9000/bucket/obj
  %s --id minio_root --delete http://127.0.0.1:9000/bucket/obj

配置文件: ~/.s3curl.json
  格式: {"profiles": {"name": {"id": "AK", "key": "SK"}}}
`, os.Args[0], os.Args[0], os.Args[0], os.Args[0])
}
