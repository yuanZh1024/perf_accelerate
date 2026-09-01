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

use strict;
use POSIX;
use Digest::MD5;
use Digest::SHA qw(sha256_hex hmac_sha256 hmac_sha256_hex);
use FindBin;
use MIME::Base64 qw(encode_base64);
use Getopt::Long qw(GetOptions);
use URI::Escape qw(uri_escape_utf8);

# 常量：stat返回数组下标
use constant STAT_MODE => 2;
use constant STAT_UID => 4;

#===================== 可自定义配置区 =====================
my @endpoints = ( 's3.amazonaws.com',
                  's3-us-west-1.amazonaws.com',
                  's3-us-west-2.amazonaws.com',
                  's3-us-gov-west-1.amazonaws.com',
                  's3-eu-west-1.amazonaws.com',
                  's3-ap-southeast-1.amazonaws.com',
                  's3-ap-northeast-1.amazonaws.com',
                  's3-sa-east-1.amazonaws.com', );

my $CURL = "curl";
#===================== 自定义配置结束 =====================

# 全局变量
my $cmdLineSecretKey;
my %awsSecretAccessKeys = ();
my $keyId;
my $secretKey;
my $contentType = "";
my $acl;
my $contentMD5 = "";
my $fileToPut;
my $createBucket;
my $doDelete;
my $doHead;
my $help;
my $debug = 0;
my $copySourceObject;
my $copySourceRange;
my $postBody;
my $calculateContentMD5 = 0;
my $servicePath = "";
my $DOTFILENAME=".s3curl";
my $region = 'us-east-1';

my $EXECFILE=$FindBin::Bin;
my $LOCALDOTFILE = $EXECFILE . "/" . $DOTFILENAME;
my $HOMEDOTFILE = $ENV{HOME} . "/" . $DOTFILENAME;
my $DOTFILE = -f $LOCALDOTFILE? $LOCALDOTFILE : $HOMEDOTFILE;

if (-f $DOTFILE) {
    open(CONFIG, $DOTFILE) || die "can't open $DOTFILE: $!";
    my @stats = stat(*CONFIG);
    if (($stats[STAT_UID] != $<) || $stats[STAT_MODE] & 066) {
        die "I refuse to read your credentials from $DOTFILE as this file is " .
            "readable by, writable by or owned by someone else. Try " .
            "chmod 600 $DOTFILE";
    }
    my @lines = <CONFIG>;
    close CONFIG;
    eval("@lines");
    die "Failed to eval() file $DOTFILE:\n$@\n" if ($@);
}

GetOptions(
    'id=s' => \$keyId,
    'key=s' => \$cmdLineSecretKey,
    'contentType=s' => \$contentType,
    'acl=s' => \$acl,
    'contentMd5=s' => \$contentMD5,
    'put=s' => \$fileToPut,
    'copySrc=s' => \$copySourceObject,
    'copySrcRange=s' => \$copySourceRange,
    'post:s' => \$postBody,
    'delete' => \$doDelete,
    'createBucket:s' => \$createBucket,
    'head' => \$doHead,
    'help' => \$help,
    'debug' => \$debug,
    'calculateContentMd5' => \$calculateContentMD5,
    'servicePath:s' => \$servicePath,
    'endpoint:s' => \@endpoints,
    'region=s' => \$region,
);

debug("endpoints: @endpoints");

my $usage = <<USAGE;
Usage $0 --id friendly-name (or AWSAccessKeyId) [options] -- [curl-options] [URL]
 options:
  --key SecretAccessKey       id/key are AWSAcessKeyId and Secret (unsafe)
  --region <region>           AWS region (default us-east-1, e.g. us-west-2)
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

if ($cmdLineSecretKey) {
    printCmdlineSecretWarning();
    sleep 5;
    $secretKey = $cmdLineSecretKey;
} else {
    my $keyinfo = $awsSecretAccessKeys{$keyId};
    die "I don't know about key with friendly name $keyId. " .
        "Do you need to set it up in $DOTFILE?"
        unless defined $keyinfo;
    $keyId = $keyinfo->{id};
    $secretKey = $keyinfo->{key};
}

if ($contentMD5 && $calculateContentMD5) {
    die "cannot specify both --contentMd5 and --calculateContentMd5";
}

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

my $resource;
my $host_header;          # 仅主机名（不含端口）
my $canonicalURI;
my $canonicalQueryString = "";
my %xamzHeaders;
$xamzHeaders{'x-amz-acl'}=$acl if (defined $acl);
$xamzHeaders{'x-amz-copy-source'}=$copySourceObject if (defined $copySourceObject);
$xamzHeaders{'x-amz-copy-source-range'}="bytes=$copySourceRange" if (defined $copySourceRange);

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

# 解析URL和参数
for (my $i=0; $i<@ARGV; $i++) {
    my $arg = $ARGV[$i];
    if ($arg =~ /https?:\/\/([^\/:?]+)(?::(\d+))?([^?]*)(?:\?(\S+))?/) {
        my $host = $1;
        my $port = defined $2 ? $2 : "";
        my $requestURI = $3;
        my $query = defined $4 ? $4 : "";
        # 重要：Host 头只取主机名，不含端口
        $host_header = $host;
        debug("Found the url: host=$host_header (without port), port=$port, uri=$requestURI, query=$query;");

        if (length $requestURI) {
            $canonicalURI = $requestURI;
        } else {
            $canonicalURI = "/";
        }

        if ($query) {
            my %params = ();
            foreach my $pair (split /&/, $query) {
                my ($key, $val) = split /=/, $pair, 2;
                $val = '' unless defined $val;
                $params{$key} = $val;
            }
            my @sorted_keys = sort keys %params;
            my @pairs = ();
            foreach my $k (@sorted_keys) {
                my $v = $params{$k};
                $v = uri_escape_utf8($v, "^A-Za-z0-9\-\._~");
                push @pairs, uri_escape_utf8($k, "^A-Za-z0-9\-\._~") . "=" . $v;
            }
            $canonicalQueryString = join "&", @pairs;
        } else {
            $canonicalQueryString = "";
        }

        # 保留原V2 resource（兼容性，但V4不用）
        if (length $requestURI) {
            $resource = $requestURI;
        } else {
            $resource = "/";
        }
        my @attributes = ();
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
    }
    elsif ($arg =~ /\-X/) {
        $method = $ARGV[++$i];
    }
    elsif ($arg =~ /\-H/) {
        my $header = $ARGV[++$i];
        if ($header =~ /^[Hh][Oo][Ss][Tt]:(.+)$/) {
            $host_header = $1;
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

# ----- 推断区域 -----
if (!defined $region || $region eq '') {
    my $region_from_host = '';
    foreach my $ep (@endpoints) {
        if ($host_header =~ /s3[.\-]([a-z0-9\-]+)\.amazonaws\.com/) {
            $region_from_host = $1;
            last;
        }
    }
    if ($region_from_host) {
        $region = $region_from_host;
        debug("Inferred region from host: $region");
    } else {
        $region = 'us-east-1';
        debug("Using default region: $region");
    }
}

# ----- 准备V4签名所需的头 -----
my $now = time;
my $amzDate = POSIX::strftime("%Y%m%dT%H%M%SZ", gmtime($now));
my $dateStamp = POSIX::strftime("%Y%m%d", gmtime($now));
if (exists $xamzHeaders{'x-amz-date'}) {
    $amzDate = $xamzHeaders{'x-amz-date'};
    $dateStamp = substr($amzDate, 0, 8);
}
$xamzHeaders{'x-amz-date'} = $amzDate;

# 计算 payload 哈希
my $payloadHash;
my $bodyData = '';   # 用于 PUT 请求的请求体（仅当有数据时）
if ($fileToPut) {
    $payloadHash = calculateFileSHA256($fileToPut);
} elsif ($createBucket) {
    $bodyData = getCreateBucketData($createBucket);
    $payloadHash = calculateStringSHA256($bodyData);
} elsif ($postBody) {
    $payloadHash = calculateFileSHA256($postBody);
} else {
    $payloadHash = sha256_hex('');
}
$xamzHeaders{'x-amz-content-sha256'} = $payloadHash;

# ----- 构建规范请求 -----
my %allHeaders;
die "Host header not found!" unless defined $host_header;
$allHeaders{'host'} = $host_header;   # 仅主机名
foreach my $h (keys %xamzHeaders) {
    $allHeaders{lc($h)} = $xamzHeaders{$h};
}
# 只有非空的 Content-Type 才加入签名和发送
if ($contentType && $contentType ne '') {
    $allHeaders{'content-type'} = $contentType;
}
if ($contentMD5) {
    $allHeaders{'content-md5'} = $contentMD5;
}

my @header_names = sort keys %allHeaders;
my $canonicalHeaders = "";
my @signed_headers = ();
foreach my $h (@header_names) {
    my $val = $allHeaders{$h};
    $val =~ s/^\s+|\s+$//g;
    $canonicalHeaders .= "$h:$val\n";
    push @signed_headers, $h;
}
my $signedHeaders = join ";", @signed_headers;

my $canonicalRequest = "$method\n$canonicalURI\n$canonicalQueryString\n$canonicalHeaders\n$signedHeaders\n$payloadHash";
debug("CanonicalRequest:\n$canonicalRequest");
my $hashedCanonicalRequest = sha256_hex($canonicalRequest);
debug("HashedCanonicalRequest: $hashedCanonicalRequest");

my $credentialScope = "$dateStamp/$region/s3/aws4_request";
my $stringToSign = "AWS4-HMAC-SHA256\n$amzDate\n$credentialScope\n$hashedCanonicalRequest";
debug("StringToSign:\n$stringToSign");

# ================== 关键修正：HMAC 参数顺序 ==================
# Digest::SHA::hmac_sha256($data, $key)  ！！！注意顺序
my $kSecret = "AWS4" . $secretKey;
my $kDate = hmac_sha256($dateStamp, $kSecret);          # 修正：$data, $key
my $kRegion = hmac_sha256($region, $kDate);
my $kService = hmac_sha256("s3", $kRegion);
my $kSigning = hmac_sha256("aws4_request", $kService);
my $signature = hmac_sha256_hex($stringToSign, $kSigning);  # 修正：$data, $key

debug("Signature: $signature");

my $authorization = "AWS4-HMAC-SHA256 Credential=$keyId/$credentialScope, SignedHeaders=$signedHeaders, Signature=$signature";

# 准备 curl 参数
my @args = ();
push @args, ("-v") if ($debug);
# 强制覆盖 Host 头（不含端口）
push @args, ("-H", "Host: $host_header");
push @args, ("-H", "x-amz-date: $amzDate");
push @args, ("-H", "Authorization: $authorization");
push @args, ("-H", "x-amz-content-sha256: $payloadHash");
push @args, ("-H", "x-amz-acl: $acl") if (defined $acl);
push @args, ("-H", "content-type: $contentType") if ($contentType && $contentType ne '');
push @args, ("-H", "Content-MD5: $contentMD5") if (length $contentMD5);
push @args, ("-L");
push @args, ("-T", $fileToPut) if (defined $fileToPut);
push @args, ("-X", "DELETE") if (defined $doDelete);
push @args, ("-X", "POST") if(defined $postBody);
push @args, ("-I") if (defined $doHead);

if (defined $createBucket) {
    if ($bodyData ne '') {
        push @args, ("--data-binary", $bodyData);
    }
    push @args, ("-X", "PUT");
} elsif (defined $copySourceObject) {
    push @args, ("-X", "PUT");
    push @args, ("-H", "x-amz-copy-source: $copySourceObject");
} elsif (defined $postBody) {
    if (length($postBody)>0) {
        push @args, ("-T", $postBody);
    }
}
push @args, @ARGV;

debug("exec $CURL " . join (" ", map { / / && qq/'$_'/ || $_ } @args));
exec($CURL, @args)  or die "can't exec program: $!";

#===================== 子函数 =====================

sub debug {
    my ($str) = @_;
    $str =~ s/\n/\\n/g;
    print STDERR "s3curl: $str\n" if ($debug);
}

sub getResourceToSign {
    my ($host, $resourceToSignRef) = @_;
    if ($servicePath) {
        $$resourceToSignRef =~ s/$servicePath//;
        debug ("resourceToSignRef: $$resourceToSignRef");
    }
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
    $$resourceToSignRef = "/$host".$$resourceToSignRef;
    debug("cname endpoint signing case");
}

sub printCmdlineSecretWarning {
    print STDERR <<END_WARNING;
WARNING: It isn't safe to put your AWS secret access key on the
command line!  The recommended key management system is to store
your AWS secret access keys in a file owned by, and only readable
by you.
For example:
\%awsSecretAccessKeys = (
    personal => {
        id => '1ME55KNV6SBTR7EXG0R2',
        key => 'zyMrlZUKeG9UcYpwzlPko/+Ciu0K2co0duRM3fhi',
    },
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

sub uri_unescape {
  my ($input) = @_;
  $input =~ s/\%([A-Fa-f0-9]{2})/pack('C', hex($1))/seg;
  debug("replaced string: " . $input);
  return ($input);
}

sub getCreateBucketData {
    my ($createBucket) = @_;
    my $data = "";
    if (length($createBucket) > 0) {
        $data = "<CreateBucketConfiguration><LocationConstraint>$createBucket</LocationConstraint></CreateBucketConfiguration>";
    }
    return $data;
}

sub calculateStringContentMD5 {
    my ($string) = @_;
    my $md5 = Digest::MD5->new;
    $md5->add($string);
    my $b64 = encode_base64($md5->digest);
    chomp($b64);
    return $b64;
}

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

sub calculateStringSHA256 {
    my ($string) = @_;
    return sha256_hex($string);
}

sub calculateFileSHA256 {
    my ($file_name) = @_;
    open(FILE, "<$file_name") || die "could not open file $file_name for SHA256 calculation";
    binmode(FILE) || die "could not set file reading to binary mode: $!";
    my $sha = Digest::SHA->new(256);
    $sha->addfile(*FILE);
    close(FILE) || die "could not close $file_name";
    return $sha->hexdigest;
}