import boto3
import re
import os

s3 = boto3.client('s3')
dynamodb = boto3.resource('dynamodb')

def lambda_handler(event, context):
    for record in event['Records']:
        bucket = record['s3']['bucket']['name']
        key = record['s3']['object']['key']
        
        # 从 key 中解析信息
        # key 格式: [4位哈希]/[服务器ID]/[年]-[月]-[日]-[时]-[分]/[客户ID]-[时间戳].data
        # 示例: a5b2/i-31cc02/2015-07-05-00-25/87423-1436055953839.data
        parts = key.split('/')
        if len(parts) < 4:
            print(f"Key format not matched: {key}")
            continue
        
        server_id = parts[1]
        timestamp_str = parts[2]  # 2015-07-05-00-25
        
        # 解析客户ID和epoch时间戳
        filename = parts[3]  # 87423-1436055953839.data
        match = re.match(r'(\d+)-(\d+)\.data', filename)
        if not match:
            print(f"Filename format not matched: {filename}")
            continue
        
        customer_id = match.group(1)
        epoch_ts = match.group(2)
        
        # 范围键 = 时间戳-服务器ID（保证唯一性）
        ts_server = f"{timestamp_str}-{server_id}"
        
        # 获取对象元数据和大小
        try:
            head = s3.head_object(Bucket=bucket, Key=key)
            size = head['ContentLength']
            # 用户自定义元数据: x-amz-meta-has-transaction
            metadata = head.get('Metadata', {})
            has_transaction = metadata.get('has-transaction', '').lower()
        except Exception as e:
            print(f"Error getting object metadata: {e}")
            continue
        
        # 确定 DynamoDB 表名（桶名 + -index 后缀）
        table_name = f"{bucket}-index"
        table = dynamodb.Table(table_name)
        
        # 构建要写入的 item
        item = {
            'CustomerID': customer_id,
            'TS_Server': ts_server,
            'ServerID': server_id,
            'S3Key': key,
            'Size': size,
            'EpochTimestamp': int(epoch_ts)
        }
        
        # 稀疏索引：只有存在交易记录时才添加 HasTransaction 属性
        if has_transaction == 'true' or has_transaction == 'yes':
            item['HasTransaction'] = 'true'
        
        # 写入 DynamoDB
        try:
            table.put_item(Item=item)
            print(f"Indexed: {key} -> Customer={customer_id}, Server={server_id}")
        except Exception as e:
            print(f"Error writing to DynamoDB: {e}")
            raise
    
    return {'statusCode': 200, 'body': 'Indexing complete'}
