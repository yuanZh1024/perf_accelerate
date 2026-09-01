package s3curl
// Package v4 实现了 AWS 签名版本 4 算法（通常称为 SigV4）。
//
// 关于 SigV4 的更多信息，请参阅 IAM 用户指南中的“签名 AWS API 请求”。
//
// 尽管此实现可以在外部上下文中工作，但它主要面向 SDK 内部使用，
// 您可能会遇到一些围绕头部规范化的边缘行为。
//
// # 预转义请求 URI
//
// AWS v4 签名验证要求规范字符串的 URI 路径部分必须是 HTTP 请求路径的转义形式。
// Go HTTP 客户端会自动对 HTTP 请求执行转义。这可能导致签名验证错误，
// 因为请求与生成签名时所依据的 URI 路径或查询字符串不一致。
// 因此，我们建议您在 SDK 外部使用此签名器时显式转义请求，以防止可能的签名不匹配。
// 可以通过设置请求的 URL.Opaque 字段来实现。签名器将优先使用该值，
// 如果未设置则回退到 URL.EscapedPath 的返回值。
// 设置 URL.Opaque 时，必须采用以下形式：
//
//	"//<hostname>/<path>"    // 例如 "//example.com/some/path"
//
// 开头的 "//" 和主机名是必需的，否则转义将无法正常工作。
// TestStandaloneSign 单元测试提供了在 SDK 外部使用签名器并预转义 URI 路径的完整示例。
package v4

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"hash"
	"net/http"
	"net/textproto"
	"net/url"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	v4Internal "github.com/aws/aws-sdk-go-v2/aws/signer/internal/v4"
	"github.com/aws/smithy-go/encoding/httpbinding"
	"github.com/aws/smithy-go/logging"
)

const (
	signingAlgorithm    = "AWS4-HMAC-SHA256"   // 固定签名算法标识
	authorizationHeader = "Authorization"      // 标准签名头
	Version             = "SigV4"              // 版本标识
)

// HTTPSigner 是 SigV4 签名器的接口，能够对 HTTP 请求进行签名
type HTTPSigner interface {
	SignHTTP(ctx context.Context, credentials aws.Credentials, r *http.Request, payloadHash string, service string, region string, signingTime time.Time, optFns ...func(*SignerOptions)) error
}

// keyDerivator 接口用于派生签名密钥
type keyDerivator interface {
	DeriveKey(credential aws.Credentials, service, region string, signingTime v4Internal.SigningTime) []byte
}

// SignerOptions 配置 SigV4 签名器的行为
type SignerOptions struct {
	// DisableHeaderHoisting 禁止签名器将 HTTP 头部键/值对从请求头移到请求的查询字符串。
	// 这通常用于预签名请求，防止头部被添加到查询字符串中。
	DisableHeaderHoisting bool

	// DisableURIPathEscaping 禁止签名器对 URI 路径进行自动转义（用于签名规范字符串的路径部分）。
	// 对于不需要额外转义的服务（如 S3），可以启用此选项。
	// 参考：http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
	DisableURIPathEscaping bool

	// Logger 用于发送日志信息
	Logger logging.Logger

	// LogSigning 启用签名过程的日志记录。
	// 会记录规范请求、待签名字符串，以及预签名时的预签名 URL。
	LogSigning bool

	// DisableSessionToken 禁止将 Session Token 作为 X-Amz-Security-Token 添加到请求中。
	// 某些 v4 变体需要将令牌放在其他位置。
	DisableSessionToken bool
}

// Signer 是实际的签名器，包含配置和密钥派生器
type Signer struct {
	options      SignerOptions
	keyDerivator keyDerivator
}

// NewSigner 创建并返回一个新的 SigV4 签名器
func NewSigner(optFns ...func(signer *SignerOptions)) *Signer {
	options := SignerOptions{}
	for _, fn := range optFns {
		fn(&options)
	}
	return &Signer{options: options, keyDerivator: v4Internal.NewSigningKeyDeriver()}
}

// httpSigner 是执行实际签名工作的内部结构，封装了签名所需的所有上下文信息
type httpSigner struct {
	Request      *http.Request
	ServiceName  string
	Region       string
	Time         v4Internal.SigningTime        // 签名时间（UTC）
	Credentials  aws.Credentials               // 访问凭证
	KeyDerivator keyDerivator                  // 密钥派生器
	IsPreSign    bool                          // 是否为预签名（即签名放到查询参数）

	PayloadHash string                         // 请求体的 SHA-256 哈希（十六进制）

	DisableHeaderHoisting  bool
	DisableURIPathEscaping bool
	DisableSessionToken    bool
}

// Build 是核心方法，执行签名计算并将结果应用到请求（修改请求对象）
func (s *httpSigner) Build() (signedRequest, error) {
	req := s.Request

	query := req.URL.Query()
	headers := req.Header

	// 设置必需的签名头部或查询参数（如 X-Amz-Date, X-Amz-Algorithm 等）
	s.setRequiredSigningFields(headers, query)

	// 对每个查询参数的多个值进行字典序排序
	for key := range query {
		sort.Strings(query[key])
	}

	// 规范化 Host 头部（去除多余空格）
	v4Internal.SanitizeHostForHeader(req)

	// 构建凭证范围（Credential Scope），格式：date/region/service/aws4_request
	credentialScope := s.buildCredentialScope()
	credentialStr := s.Credentials.AccessKeyID + "/" + credentialScope
	if s.IsPreSign {
		// 预签名时，将凭证信息放入查询参数
		query.Set(v4Internal.AmzCredentialKey, credentialStr)
	}

	unsignedHeaders := headers
	if s.IsPreSign && !s.DisableHeaderHoisting {
		// 预签名且未禁用头部上提时，将允许的头部移到查询字符串
		var urlValues url.Values
		urlValues, unsignedHeaders = buildQuery(v4Internal.AllowedQueryHoisting, headers)
		for k := range urlValues {
			query[k] = urlValues[k]
		}
	}

	// 确定 Host 值（优先使用 req.Host，否则使用 URL.Host）
	host := req.URL.Host
	if len(req.Host) > 0 {
		host = req.Host
	}

	// 构建规范头部（CanonicalHeaders）和签名头部列表（SignedHeaders）
	// 返回：signed（实际签名的头部映射），signedHeadersStr（分号分隔的头部名），canonicalHeaderStr（规范头部字符串）
	signedHeaders, signedHeadersStr, canonicalHeaderStr := s.buildCanonicalHeaders(host, v4Internal.IgnoredHeaders, unsignedHeaders, s.Request.ContentLength)

	if s.IsPreSign {
		// 预签名时，将签名头部列表放入查询参数
		query.Set(v4Internal.AmzSignedHeadersKey, signedHeadersStr)
	}

	// 构建规范化查询字符串（替换 + 为 %20）
	var rawQuery strings.Builder
	rawQuery.WriteString(strings.Replace(query.Encode(), "+", "%20", -1))

	// 获取 URI 路径，并根据配置决定是否转义
	canonicalURI := v4Internal.GetURIPath(req.URL)
	if !s.DisableURIPathEscaping {
		canonicalURI = httpbinding.EscapePath(canonicalURI, false)
	}

	// 组装规范请求（Canonical Request）
	canonicalString := s.buildCanonicalString(
		req.Method,
		canonicalURI,
		rawQuery.String(),
		signedHeadersStr,
		canonicalHeaderStr,
	)

	// 构建待签名字符串（StringToSign）
	strToSign := s.buildStringToSign(credentialScope, canonicalString)

	// 计算最终签名
	signingSignature, err := s.buildSignature(strToSign)
	if err != nil {
		return signedRequest{}, err
	}

	// 将签名添加到请求（预签名放查询参数，普通签名放 Authorization 头）
	if s.IsPreSign {
		rawQuery.WriteString("&X-Amz-Signature=")
		rawQuery.WriteString(signingSignature)
	} else {
		headers[authorizationHeader] = append(headers[authorizationHeader][:0], buildAuthorizationHeader(credentialStr, signedHeadersStr, signingSignature))
	}

	req.URL.RawQuery = rawQuery.String()

	return signedRequest{
		Request:         req,
		SignedHeaders:   signedHeaders,
		CanonicalString: canonicalString,
		StringToSign:    strToSign,
		PreSigned:       s.IsPreSign,
	}, nil
}

// buildAuthorizationHeader 构造 Authorization 头的值
func buildAuthorizationHeader(credentialStr, signedHeadersStr, signingSignature string) string {
	const credential = "Credential="
	const signedHeaders = "SignedHeaders="
	const signature = "Signature="
	const commaSpace = ", "

	var parts strings.Builder
	parts.Grow(len(signingAlgorithm) + 1 +
		len(credential) + len(credentialStr) + 2 +
		len(signedHeaders) + len(signedHeadersStr) + 2 +
		len(signature) + len(signingSignature))
	parts.WriteString(signingAlgorithm)
	parts.WriteRune(' ')
	parts.WriteString(credential)
	parts.WriteString(credentialStr)
	parts.WriteString(commaSpace)
	parts.WriteString(signedHeaders)
	parts.WriteString(signedHeadersStr)
	parts.WriteString(commaSpace)
	parts.WriteString(signature)
	parts.WriteString(signingSignature)
	return parts.String()
}

// SignHTTP 对 HTTP 请求应用 AWS v4 签名（使用头部携带签名）
// 参数 payloadHash 必须是请求体的 SHA-256 十六进制串，若无请求体则为空串的 SHA-256。
// 此方法会修改传入的请求对象。
func (s Signer) SignHTTP(ctx context.Context, credentials aws.Credentials, r *http.Request, payloadHash string, service string, region string, signingTime time.Time, optFns ...func(options *SignerOptions)) error {
	options := s.options
	for _, fn := range optFns {
		fn(&options)
	}

	signer := &httpSigner{
		Request:                r,
		PayloadHash:            payloadHash,
		ServiceName:            service,
		Region:                 region,
		Credentials:            credentials,
		Time:                   v4Internal.NewSigningTime(signingTime.UTC()),
		DisableHeaderHoisting:  options.DisableHeaderHoisting,
		DisableURIPathEscaping: options.DisableURIPathEscaping,
		DisableSessionToken:    options.DisableSessionToken,
		KeyDerivator:           s.keyDerivator,
	}

	signedRequest, err := signer.Build()
	if err != nil {
		return err
	}

	logSigningInfo(ctx, options, &signedRequest, false)
	return nil
}

// PresignHTTP 生成预签名 URL，签名信息放在查询参数中。
// 返回签名后的 URL 字符串以及需要包含的头部映射（这些头部必须在实际请求时携带）。
// 注意：此方法不会修改传入的请求对象，而是克隆一份进行操作。
// 预签名 URL 的有效期需要通过添加 X-Amz-Expires 查询参数指定（单位秒）。
func (s *Signer) PresignHTTP(
	ctx context.Context, credentials aws.Credentials, r *http.Request,
	payloadHash string, service string, region string, signingTime time.Time,
	optFns ...func(*SignerOptions),
) (signedURI string, signedHeaders http.Header, err error) {
	options := s.options
	for _, fn := range optFns {
		fn(&options)
	}

	// 克隆请求以避免修改原始请求
	signer := &httpSigner{
		Request:                r.Clone(r.Context()),
		PayloadHash:            payloadHash,
		ServiceName:            service,
		Region:                 region,
		Credentials:            credentials,
		Time:                   v4Internal.NewSigningTime(signingTime.UTC()),
		IsPreSign:              true,
		DisableHeaderHoisting:  options.DisableHeaderHoisting,
		DisableURIPathEscaping: options.DisableURIPathEscaping,
		DisableSessionToken:    options.DisableSessionToken,
		KeyDerivator:           s.keyDerivator,
	}

	signedRequest, err := signer.Build()
	if err != nil {
		return "", nil, err
	}

	logSigningInfo(ctx, options, &signedRequest, true)

	// 对返回的头部进行规范化（键名转为标准 MIME 格式）
	signedHeaders = make(http.Header)
	for k, v := range signedRequest.SignedHeaders {
		key := textproto.CanonicalMIMEHeaderKey(k)
		signedHeaders[key] = append(signedHeaders[key], v...)
	}

	return signedRequest.Request.URL.String(), signedHeaders, nil
}

// buildCredentialScope 构建凭证范围字符串
func (s *httpSigner) buildCredentialScope() string {
	return v4Internal.BuildCredentialScope(s.Time, s.Region, s.ServiceName)
}

// buildQuery 根据规则将允许的头部转移到查询字符串（用于预签名时的头部上提）
func buildQuery(r v4Internal.Rule, header http.Header) (url.Values, http.Header) {
	query := url.Values{}
	unsignedHeaders := http.Header{}

	// 某些头部需要转为小写以兼容 S3 的特殊要求
	lowerCaseHeaders := map[string]string{
		"X-Amz-Expected-Bucket-Owner": "x-amz-expected-bucket-owner",
		"X-Amz-Request-Payer":         "x-amz-request-payer",
	}

	for k, h := range header {
		if newKey, ok := lowerCaseHeaders[k]; ok {
			k = newKey
		}
		if r.IsValid(k) {
			query[k] = h
		} else {
			unsignedHeaders[k] = h
		}
	}
	return query, unsignedHeaders
}

// buildCanonicalHeaders 构建规范头部字符串和签名头部列表。
// 返回：signed（被签名的头部映射），signedHeaders（分号分隔的头部名），canonicalHeadersStr（规范头部文本）。
func (s *httpSigner) buildCanonicalHeaders(host string, rule v4Internal.Rule, header http.Header, length int64) (signed http.Header, signedHeaders, canonicalHeadersStr string) {
	signed = make(http.Header)

	var headers []string
	const hostHeader = "host"
	headers = append(headers, hostHeader)
	signed[hostHeader] = append(signed[hostHeader], host)

	// 如果内容长度 > 0，则 content-length 头部也要参与签名
	const contentLengthHeader = "content-length"
	if length > 0 {
		headers = append(headers, contentLengthHeader)
		signed[contentLengthHeader] = append(signed[contentLengthHeader], strconv.FormatInt(length, 10))
	}

	// 遍历所有头部，按规则决定是否参与签名
	for k, v := range header {
		if !rule.IsValid(k) {
			continue // 忽略的头部不参与签名
		}
		if strings.EqualFold(k, contentLengthHeader) {
			continue // 已经处理过 content-length，避免重复
		}

		lowerCaseKey := strings.ToLower(k)
		if _, ok := signed[lowerCaseKey]; ok {
			// 如果已存在，则追加值（处理多值头部）
			signed[lowerCaseKey] = append(signed[lowerCaseKey], v...)
			continue
		}
		headers = append(headers, lowerCaseKey)
		signed[lowerCaseKey] = v
	}
	sort.Strings(headers) // 按字典序排序

	signedHeaders = strings.Join(headers, ";")

	// 构建规范头部字符串，格式：header:value\n
	var canonicalHeaders strings.Builder
	n := len(headers)
	const colon = ':'
	for i := 0; i < n; i++ {
		if headers[i] == hostHeader {
			canonicalHeaders.WriteString(hostHeader)
			canonicalHeaders.WriteRune(colon)
			canonicalHeaders.WriteString(v4Internal.StripExcessSpaces(host))
		} else {
			canonicalHeaders.WriteString(headers[i])
			canonicalHeaders.WriteRune(colon)
			values := signed[headers[i]]
			for j, v := range values {
				cleanedValue := strings.TrimSpace(v4Internal.StripExcessSpaces(v))
				canonicalHeaders.WriteString(cleanedValue)
				if j < len(values)-1 {
					canonicalHeaders.WriteRune(',')
				}
			}
		}
		canonicalHeaders.WriteRune('\n')
	}
	canonicalHeadersStr = canonicalHeaders.String()

	return signed, signedHeaders, canonicalHeadersStr
}

// buildCanonicalString 组装规范请求字符串
func (s *httpSigner) buildCanonicalString(method, uri, query, signedHeaders, canonicalHeaders string) string {
	return strings.Join([]string{
		method,
		uri,
		query,
		canonicalHeaders,
		signedHeaders,
		s.PayloadHash,
	}, "\n")
}

// buildStringToSign 构建待签名字符串
func (s *httpSigner) buildStringToSign(credentialScope, canonicalRequestString string) string {
	return strings.Join([]string{
		signingAlgorithm,
		s.Time.TimeFormat(),                              // YYYYMMDD'T'HHMMSS'Z'
		credentialScope,
		hex.EncodeToString(makeHash(sha256.New(), []byte(canonicalRequestString))),
	}, "\n")
}

// makeHash 辅助函数，计算数据的哈希值
func makeHash(hash hash.Hash, b []byte) []byte {
	hash.Reset()
	hash.Write(b)
	return hash.Sum(nil)
}

// buildSignature 使用派生密钥计算签名
func (s *httpSigner) buildSignature(strToSign string) (string, error) {
	key := s.KeyDerivator.DeriveKey(s.Credentials, s.ServiceName, s.Region, s.Time)
	return hex.EncodeToString(v4Internal.HMACSHA256(key, []byte(strToSign))), nil
}

// setRequiredSigningFields 根据签名类型（预签名或普通）设置必需的头部或查询参数
func (s *httpSigner) setRequiredSigningFields(headers http.Header, query url.Values) {
	amzDate := s.Time.TimeFormat()

	if s.IsPreSign {
		query.Set(v4Internal.AmzAlgorithmKey, signingAlgorithm)
		sessionToken := s.Credentials.SessionToken
		if !s.DisableSessionToken && len(sessionToken) > 0 {
			query.Set("X-Amz-Security-Token", sessionToken)
		}
		query.Set(v4Internal.AmzDateKey, amzDate)
		return
	}

	headers[v4Internal.AmzDateKey] = append(headers[v4Internal.AmzDateKey][:0], amzDate)
	if !s.DisableSessionToken && len(s.Credentials.SessionToken) > 0 {
		headers[v4Internal.AmzSecurityTokenKey] = append(headers[v4Internal.AmzSecurityTokenKey][:0], s.Credentials.SessionToken)
	}
}

// logSigningInfo 记录签名相关信息（规范请求、待签名字符串等）
func logSigningInfo(ctx context.Context, options SignerOptions, request *signedRequest, isPresign bool) {
	if !options.LogSigning {
		return
	}
	signedURLMsg := ""
	if isPresign {
		signedURLMsg = fmt.Sprintf(logSignedURLMsg, request.Request.URL.String())
	}
	logger := logging.WithContext(ctx, options.Logger)
	logger.Logf(logging.Debug, logSignInfoMsg, request.CanonicalString, request.StringToSign, signedURLMsg)
}

// signedRequest 封装签名后的结果，包含签名后的请求和相关元数据
type signedRequest struct {
	Request         *http.Request
	SignedHeaders   http.Header
	CanonicalString string
	StringToSign    string
	PreSigned       bool
}

// 日志模板
const logSignInfoMsg = `Request Signature:
---[ CANONICAL STRING  ]-----------------------------
%s
---[ STRING TO SIGN ]--------------------------------
%s%s
-----------------------------------------------------`
const logSignedURLMsg = `
---[ SIGNED URL ]------------------------------------
%s`