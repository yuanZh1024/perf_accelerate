#!/usr/bin/perl -w
# Copyright 2006-2010 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may not use this
# file except in compliance with the License. A copy of the License is located at
#
#     http://aws.amazon.com/apache2.0/
#
# or in the "license" file accompanying this file. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
# for the specific language governing permissions and limitations under the License.

use strict;           # 开启perl严格语法检查，未定义变量直接报错
use POSIX;            # 用于gmtime UTC时间格式化，生成http标准日期
# 需要cpan安装的加密模块
use Digest::HMAC_SHA1;# SigV2签名算法 HMAC‑SHA1
use Digest::MD5;      # 计算Content‑MD5消息摘要
use FindBin;          # 获取脚本所在目录
use MIME::Base64 qw(encode_base64); # base64编码签名结果
use Getopt::Long qw(GetOptions);    # 解析命令行参数

# 常量：stat返回数组下标
use constant STAT_MODE => 2; # stat返回数组下标：文件权限mode
use constant STAT_UID => 4;  # stat返回数组下标：文件所有者uid

#===================== 可自定义配置区 =====================
# AWS S3官方域名列表，用于判断虚拟主机风格bucket
my @endpoints = ( 's3.amazonaws.com',
                  's3-us-west-1.amazonaws.com',
                  's3-us-west-2.amazonaws.com',
                  's3-us-gov-west-1.amazonaws.com',
                  's3-eu-west-1.amazonaws.com',
                  's3-ap-southeast-1.amazonaws.com',
                  's3-ap-northeast-1.amazonaws.com',
                  's3-sa-east-1.amazonaws.com', );

my $CURL = "curl"; # 底层调用curl二进制程序
#===================== 自定义配置结束 =====================

# 全局变量
my $cmdLineSecretKey;          # 命令行传入的secretKey（不安全，不推荐）
my %awsSecretAccessKeys = ();  # 多账号AK/SK哈希，从~/.s3curl配置文件加载，key为账号别名
my $keyFriendlyName;           # 账号友好别名
my $keyId;                     # AccessKeyId
my $secretKey;                 # SecretAccessKey
my $contentType = "";          # http Content‑Type
my $acl;                       # S3预定义ACL，public‑read等
my $contentMD5 = "";           # Content‑MD5 请求头
my $fileToPut;                 # PUT上传的本地文件路径
my $createBucket;              # 创建bucket，可选传入region
my $doDelete;                  # 是否执行DELETE请求
my $doHead;                    # 是否执行HEAD请求
my $help;                      # 是否打印帮助
my $debug = 0;                 # debug调试开关
my $copySourceObject;          # CopyObject复制源对象 bucket/key
my $copySourceRange;           # 对象复制字节范围 bytes=start‑end
my $postBody;                  # POST请求本地文件
my $calculateContentMD5 = 0;   # 是否自动计算MD5
my $servicePath = "";          # 要从签名资源中剔除的路径前缀
my $DOTFILENAME=".s3curl";     # 配置文件名

# 配置文件优先级：脚本同目录/.s3curl > 用户家目录 ~/.s3curl
my $EXECFILE=$FindBin::Bin;
my $LOCALDOTFILE = $EXECFILE . "/" . $DOTFILENAME;
my $HOMEDOTFILE = $ENV{HOME} . "/" . $DOTFILENAME;
my $DOTFILE = -f $LOCALDOTFILE? $LOCALDOTFILE : $HOMEDOTFILE;

#======== 读取并eval执行.s3curl配置文件 =========
if (-f $DOTFILE) {
    open(CONFIG, $DOTFILE) || die "can't open $DOTFILE: $!";
    my @stats = stat(*CONFIG);
    # 安全校验：配置文件必须属主本人，权限不能组/其他可读可写，防止密钥泄露
    if (($stats[STAT_UID] != $<) || $stats[STAT_MODE] & 066) {
        die "I refuse to read your credentials from $DOTFILE as this file is " .
            "readable by, writable by or owned by someone else. Try " .
            "chmod 600 $DOTFILE";
    }
    my @lines = <CONFIG>;
    close CONFIG;
    eval("@lines"); # 把配置文件当作perl代码执行，填充%config、%awsSecretAccessKeys
    die "Failed to eval() file $DOTFILE:\n$@\n" if ($@);
}

#======== 解析命令行参数 =========
GetOptions(
    'id=s' => \$keyId,                 # --id 指定账号别名，取自%awsSecretAccessKeys
    'key=s' => \$cmdLineSecretKey,     # --key 命令行传入secretKey（危险）
    'contentType=s' => \$contentType,  # --contentType 设置Content‑Type
    'acl=s' => \$acl,                  # --acl 设置预定义ACL
    'contentMd5=s' => \$contentMD5,    # --contentMd5 手动指定MD5
    'put=s' => \$fileToPut,            # --put file PUT上传本地文件
    'copySrc=s' => \$copySourceObject, # --copySrc bucket/key 复制源对象
    'copySrcRange=s' => \$copySourceRange, # --copySrcRange 复制字节区间
    'post:s' => \$postBody,            # --post [file] POST请求
    'delete' => \$doDelete,            # --delete DELETE方法
    'createBucket:s' => \$createBucket,# --createBucket [region] 创建bucket
    'head' => \$doHead,                # --head HEAD请求
    'help' => \$help,                  # --help 帮助
    'debug' => \$debug,                # --debug 开启调试输出
    'calculateContentMd5' => \$calculateContentMD5, #自动计算MD5
    'servicePath:s' => \$servicePath,  # --servicePath 签名剔除路径前缀
    'endpoint:s' => \@endpoints,       # --endpoint 追加S3域名列表
);

debug("endpoints: @endpoints");

# 使用帮助文本
my $usage = <<USAGE;
Usage $0 --id friendly-name (or AWSAccessKeyId) [options] -- [curl-options] [URL]
 options:
  --key SecretAccessKey       id/key are AWSAcessKeyId and Secret (unsafe)
  --contentType text/plain    set content-type header
  --acl public-read           use a 'canned' ACL (x-amz-acl header)
  --contentMd5 content_md5    add Content-MD5 header
  --calculateContentMd5       calculate Content-MD5 and add it
  --put <filename>            PUT request (from the provided local file)
  --copySrc bucket/key        Copy from this source key
  --copySrcRange {startIndex}-{endIndex}
  --post [<filename>]         POST request (optional local file)
  --delete                    DELETE request
  --createBucket [<region>]   create-bucket with optional location constraint
  --head                      HEAD request
  --debug                     enable debug logging
  --servicePath               service path which is not part of resource
  --endpoint                  add endpoint to be excluded from signed string. Specify multiple parameters if you need add more than one.
 common curl options:
  -H 'x-amz-acl: public-read' another way of using canned ACLs
  -v                          verbose logging
USAGE
die $usage if $help || !defined $keyId;

#======== 获取AK/SK =========
if ($cmdLineSecretKey) {
    printCmdlineSecretWarning(); # 命令行传入密钥打印告警
    sleep 5;
    $secretKey = $cmdLineSecretKey;
} else {
    # 从配置文件哈希根据别名查找账号信息
    my $keyinfo = $awsSecretAccessKeys{$keyId};
    die "I don't know about key with friendly name $keyId. " .
        "Do you need to set it up in $DOTFILE?"
        unless defined $keyinfo;
    $keyId = $keyinfo->{id};     # 取出真实AccessKeyId
    $secretKey = $keyinfo->{key};# 取出真实SecretAccessKey
}

if ($contentMD5 && $calculateContentMD5) {
    die "cannot specify both --contentMd5 and --calculateContentMd5";
}

#======== 判断HTTP Method =========
my $method = "";
if (defined $fileToPut or defined $createBucket or defined $copySourceObject) {
    $method = "PUT";
} elsif (defined $doDelete) {
    $method = "DELETE";
} elsif (defined $doHead) {
    $method = "HEAD";
} elsif (defined $postBody) {
    $method = "POST";
} else {
    $method = "GET";
}

my $resource; # S3签名资源路径（SigV2核心字段）
my $host;     # 请求Host

#======== 自动计算Content‑MD5 =========
if ($calculateContentMD5) {
    if ($fileToPut) {
        $contentMD5 = calculateFileContentMD5($fileToPut);
    } elsif ($createBucket) {
        $contentMD5 = calculateStringContentMD5(getCreateBucketData($createBucket));
    } elsif ($postBody) {
        $contentMD5 = calculateFileContentMD5($postBody);
    } else {
        $contentMD5 = calculateStringContentMD5('');
    }
}

# 存放所有x‑amz‑*自定义头，SigV2需要把这些头参与签名
my %xamzHeaders;
$xamzHeaders{'x-amz-acl'}=$acl if (defined $acl);
$xamzHeaders{'x-amz-copy-source'}=$copySourceObject if (defined $copySourceObject);
$xamzHeaders{'x-amz-copy-source-range'}="bytes=$copySourceRange" if (defined $copySourceRange);

#======== 解析命令行尾部URL、curl参数，提取host、resource、query参数 =========
for (my $i=0; $i<@ARGV; $i++) {
    my $arg = $ARGV[$i];
    # 正则匹配 http/https url，解析 host port path query
    if ($arg =~ /https?:\/\/([^\/:?]+)(?::(\d+))?([^?]*)(?:\?(\S+))?/) {
        $host = $1 if !$host;
        my $port = defined $2 ? $2 : "";
        my $requestURI = $3;
        my $query = defined $4 ? $4 : "";
        debug("Found the url: host=$host; port=$port; uri=$requestURI; query=$query;");
        if (length $requestURI) {
            $resource = $requestURI;
        } else {
            $resource = "/";
        }
        my @attributes = ();
        # 提取S3特殊查询参数（versionId、uploads等），拼入resource参与签名
        for my $attribute ("acl", "delete", "location", "logging", "notification",
            "partNumber", "policy", "requestPayment", "response-cache-control",
            "response-content-disposition", "response-content-encoding", "response-content-language",
            "response-content-type", "response-expires", "torrent",
            "uploadId", "uploads", "versionId", "versioning", "versions", "website", "lifecycle", "restore") {
            if ($query =~ /(?:^|&)($attribute)=?([^&]+)?(?:&|$)/) {
                my $kv_pair = sprintf("%s%s", $1, $2 ? sprintf("=%s", $2) : '');
                push @attributes, uri_unescape($kv_pair);
            }
        }
        if (@attributes) {
            $resource .= "?" . join("&", @attributes);
        }
        # 处理虚拟主机bucket格式 bucket.s3.amazonaws.com，转换为path‑style签名资源路径
        getResourceToSign($host, \$resource);
    }
    elsif ($arg =~ /\-X/) {
        # 捕获curl -X 手动指定method
        $method = $ARGV[++$i];
    }
    elsif ($arg =~ /\-H/) {
        my $header = $ARGV[++$i];
        # 解析curl传入‑H header，提取host、x‑amz‑*头
        if ($header =~ /^[Hh][Oo][Ss][Tt]:(.+)$/) {
            $host = $1;
        }
        elsif ($header =~ /^([Xx]-[Aa][Mm][Zz]-[^:]+): *(.+)$/) {
            my $name = lc $1;
            my $value = $2;
            if (exists $xamzHeaders{$name}) {
                $value = $xamzHeaders{$name} . "," . $value;
            }
            $xamzHeaders{$name} = $value;
        }
    }
}

die "Couldn't find resource by digging through your curl command line args!"
    unless defined $resource;

#======== SigV2：拼接x‑amz‑*头字符串，按字典序排序 =========
my $xamzHeadersToSign = "";
foreach (sort (keys %xamzHeaders)) {
    my $headerValue = $xamzHeaders{$_};
    $xamzHeadersToSign .= "$_:$headerValue\n";
}

#======== SigV2 使用Date头；如果外部传入x‑amz‑date就不用Date =========
my $httpDate = (defined $xamzHeaders{'x-amz-date'}) ? '' : POSIX::strftime("%a, %d %b %Y %H:%M:%S +0000", gmtime);

#===================== ✨AWS SigV2 待签名字符串 StringToSign 核心=====================
# SigV2标准格式：
# Method\n
# Content‑MD5\n
# Content‑Type\n
# Date\n
# x‑amz‑header1:value1\n
# x‑amz‑header2:value2\n
# /bucket/object?subresource
my $stringToSign = "$method\n$contentMD5\n$contentType\n$httpDate\n$xamzHeadersToSign$resource";
debug("StringToSign='" . $stringToSign . "'");

#======== HMAC‑SHA1计算签名，base64编码输出 =========
my $hmac = Digest::HMAC_SHA1->new($secretKey);
$hmac->add($stringToSign);
my $signature = encode_base64($hmac->digest, "");

#======== 组装curl参数数组 =========
my @args = ();
push @args, ("-v") if ($debug);
push @args, ("-H", "Date: $httpDate") if ($httpDate);
# SigV2 Authorization 请求头格式："AWS AccessKeyId:Base64(HMAC‑SHA1签名)"
push @args, ("-H", "Authorization: AWS $keyId:$signature");
push @args, ("-H", "x-amz-acl: $acl") if (defined $acl);
push @args, ("-L"); # curl跟随重定向
push @args, ("-H", "content-type: $contentType") if (defined $contentType);
push @args, ("-H", "Content-MD5: $contentMD5") if (length $contentMD5);
push @args, ("-T", $fileToPut) if (defined $fileToPut);
push @args, ("-X", "DELETE") if (defined $doDelete);
push @args, ("-X", "POST") if(defined $postBody);
push @args, ("-I") if (defined $doHead);

# createBucket特殊逻辑：PUT请求携带CreateBucketConfiguration XML
if (defined $createBucket) {
    my $data = getCreateBucketData($createBucket);
    push @args, ("--data-binary", $data);
    push @args, ("-X", "PUT");
} elsif (defined $copySourceObject) {
    # CopyObject，PUT请求配合x‑amz‑copy‑source头
    push @args, ("-X", "PUT");
    push @args, ("-H", "x-amz-copy-source: $copySourceObject");
} elsif (defined $postBody) {
    if (length($postBody)>0) {
        push @args, ("-T", $postBody);
    }
}
push @args, @ARGV; # 追加原始命令行curl参数与URL

debug("exec $CURL " . join (" ", map { / / && qq/'$_'/ || $_ } @args));
exec($CURL, @args)  or die "can't exec program: $!"; # exec调用curl发起真实http请求

#===================== 子函数 =====================

# debug打印日志
sub debug {
    my ($str) = @_;
    $str =~ s/\n/\\n/g;
    print STDERR "s3curl: $str\n" if ($debug);
}

# 处理虚拟主机风格bucket，转换签名resource
sub getResourceToSign {
    my ($host, $resourceToSignRef) = @_;
    if ($servicePath) {
        $$resourceToSignRef =~ s/$servicePath//;
        debug ("resourceToSignRef: $$resourceToSignRef");
    }
    # 匹配 *.s3.amazonaws.com 虚拟主机域名
    for my $ep (@endpoints) {
        if ($host =~ /(.*)\.$ep/) {
            my $vanityBucket = $1;
            $$resourceToSignRef = "/$vanityBucket".$$resourceToSignRef;
            debug("vanity endpoint signing case");
            return;
        }
        elsif ($host eq $ep) {
            debug("ordinary endpoint signing case");
            return;
        }
    }
    # CNAME自定义域名场景
    $$resourceToSignRef = "/$host".$$resourceToSignRef;
    debug("cname endpoint signing case");
}

# 命令行传入‑‑key直接给密钥的警告
sub printCmdlineSecretWarning {
    print STDERR <<END_WARNING;
WARNING: It isn't safe to put your AWS secret access key on the
command line!  The recommended key management system is to store
your AWS secret access keys in a file owned by, and only readable
by you.
For example:
\%awsSecretAccessKeys = (
    # personal account
    personal => {
        id => '1ME55KNV6SBTR7EXG0R2',
        key => 'zyMrlZUKeG9UcYpwzlPko/+Ciu0K2co0duRM3fhi',
    },
    # corporate account
    company => {
        id => '1ATXQ3HHA59CYF1CVS02',
        key => 'WQY4SrSS95pJUT95V6zWea01gBKBCL6PI0cdxeH8',
    },
);
\$ chmod 600 $DOTFILE
Will sleep and continue despite this problem.
Please set up $DOTFILE for future requests.
END_WARNING
}

# url解码
sub uri_unescape {
  my ($input) = @_;
  $input =~ s/\%([A-Fa-f0-9]{2})/pack('C', hex($1))/seg;
  debug("replaced string: " . $input);
  return ($input);
}

# 生成创建bucket请求XML，带LocationConstraint
sub getCreateBucketData {
    my ($createBucket) = @_;
    my $data = "";
    if (length($createBucket) > 0) {
        $data = "<CreateBucketConfiguration><LocationConstraint>$createBucket</LocationConstraint></CreateBucketConfiguration>";
    }
    return $data;
}

# 计算字符串MD5，返回base64编码结果（用于Content‑MD5头）
sub calculateStringContentMD5 {
    my ($string) = @_;
    my $md5 = Digest::MD5->new;
    $md5->add($string);
    my $b64 = encode_base64($md5->digest);
    chomp($b64);
    return $b64;
}

# 读取本地文件计算MD5，返回base64编码
sub calculateFileContentMD5 {
    my ($file_name) = @_;
    open(FILE, "<$file_name") || die "could not open file $file_name for MD5 calculation";
    binmode(FILE) || die "could not set file reading to binary mode: $!";
    my $md5 = Digest::MD5->new;
    $md5->addfile(*FILE);
    close(FILE) || die "could not close $file_name";
    my $b64 = encode_base64($md5->digest);
    chomp($b64);
    return $b64;
}
