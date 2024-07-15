use poor;	-- 进入数据库

SHOW TABLES;	-- 查看可用表

DROP table completed_files;	-- 删除表

DELETE FROM upload_status_files;	-- 删除表内所有数据

SELECT * from upload_status_files;	-- 查看表内数据

INSERT INTO users (username, password_hash, email) VALUES	-- 插入数据
(`123`, `123`, `123`);

-- 用户信息表
CREATE TABLE `users` (
    `user_id` 		INT(11) NOT NULL AUTO_INCREMENT comment"用户id", -- 用户ID
    `account` 		VARCHAR(11) NOT NULL UNIQUE comment"用户账号",	-- 用户账号：注册时分配给用户
	`user_name` 	VARCHAR(255) NOT NULL comment"用户名", -- 用户昵称
	`password_hash` VARCHAR(255) NOT NULL comment"md5密码", -- 密码md5加密
	`email` 		VARCHAR(255) comment"邮箱", -- 邮箱
	`phone_number` 	VARCHAR(20) comment"手机号", -- 手机号码
    `id_number` 	VARCHAR(20) comment"身份证号", -- 身份证号
    
	`registration_date` DATETIME NOT NULL comment"注册日期", -- 注册日期
	`last_login` 	DATETIME comment"最后一次登录时间", -- 最后一次登录时间
    
	`role` 			ENUM('root', 'user') NOT NULL DEFAULT 'user' comment"用户权限", -- 用户权限
    `user_status` 	ENUM('inactive', 'active', 'inactive_long', 'deleted', 'banned') NOT NULL DEFAULT 'inactive',	-- 用户状态：未激活 , '正常', '长时间未登录', '注销', '封禁中'
    
    `avatar_url` 	VARCHAR(255) comment"头像链接",
    
    PRIMARY KEY (`user_id`),
	UNIQUE INDEX `username_UNIQUE` (`user_name` ASC) VISIBLE,	-- 索引，用来快速查询
	UNIQUE INDEX `email_UNIQUE` (`email` ASC) VISIBLE	-- 索引，用来快速查询
);

-- 用户登录日志
CREATE TABLE `log_user_login`(
  `log_user_login_id` 	int(11) NOT NULL AUTO_INCREMENT comment"日志id",
  `user_id` 	int(11) NOT NULL comment"用户id", -- 用户ID
  `login_time` 	datetime NOT NULL comment"登录时间", -- 登录时间
  `client_id` 	varchar(45) NOT NULL comment"登录客户端ID", -- 登录客户端ID
  `device_info` varchar(45) comment"登录设备信息", -- 设备信息
  `location` 	varchar(45) comment"登录位置", -- 地理位置
  
  PRIMARY KEY (`log_user_login_id`),
  
  FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`)	-- 外键 users表 用户ID
);

-- 客户端链接和断开日志
CREATE TABLE `log_client_connection` (
  `log_client_connection_id` int(11) NOT NULL AUTO_INCREMENT comment"日志id", -- 自增主键
  `client_id` 	varchar(45) NOT NULL comment"链接的用户id", -- 客户端ID
  `now_time` 	datetime NOT NULL comment"开始链接时间", -- 连接时间
  `ip_address` 	VARCHAR(45) NOT NULL comment"链接ip", -- IP地址
  `status` 		enum('connected','disconnected') NOT NULL comment"链接状态", -- 连接状态：connected链接，disconnected断开
  
  PRIMARY KEY (`log_client_connection_id`)
);

-- file上传 活动文件：记录所有上传中的文件，用于记录断点和缺失块，方便实现断点续传和文件完整
CREATE TABLE `upload_status_files` (
    `file_id` 		varchar(255),			-- 文件ID，外键
    `account` 		VARCHAR(11) NOT NULL,	-- 用户账号
    `client_id` 	varchar(45) NOT NULL,	-- 客户端ID，防止多客户端同时上传文件
    `actual_file_path` VARCHAR(255) NOT NULL,	-- 文件在服务端存储路径
    `file_size` 	BIGINT NOT NULL,	-- 文件总大小
    `last_upload_time` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,	-- 上次上传时间，用来标记断联时间
    `missing_parts` TEXT,	-- 缺失的文件块，使用字符串保存数字列表 表示未上传的文件块，使用数字1~n，空格进行间隔
    
    PRIMARY KEY (`file_id`),	-- 主键
    
    FOREIGN KEY (`file_id`) REFERENCES `completed_files`(`file_id`),	-- 外键：completed_files表 文件id
	FOREIGN KEY (`account`) REFERENCES `users`(`account`)           	-- 外键：users表 用户id
);

-- 用户上传文件表：记录用户上传完成的文件
CREATE TABLE `completed_files` (
    `file_id` 		varchar(255),		-- 文件ID（服务器保存用的文件名）
    `account` 		VARCHAR(11) NOT NULL,	-- 用户账号
    `file_name` 	VARCHAR(255) NOT NULL,			-- 原文件名
    `file_format` 	VARCHAR(50) NOT NULL,			-- 文件拓展名	：txt、jpg
    `file_size` 	BIGINT NOT NULL,					-- 文件大小
    `upload_time` 	TIMESTAMP DEFAULT CURRENT_TIMESTAMP,	-- 上传时间：以第一次上传时间为准
    `actual_file_path` VARCHAR(255) NOT NULL,		-- 文件在服务端保存路径（含文件名）
    `client_display_path` VARCHAR(255),			-- 客户端展示路径（含文件名）
    `file_status` 	ENUM('uploading', 'upload_failed', 'normal', 'deleted', 'blocked', 'corrupted', 'lost'),	-- 状态可以是上传中‘uploading’，上传失败’upload_failed‘，正常'normal', 删除'deleted', 被屏蔽'blocked', 损坏'corrupted', 丢失'lost'
	
    PRIMARY KEY (`file_id`),	-- 主键

    FOREIGN KEY (`account`) REFERENCES `users`(`account`)           -- 外键关联到用户表用户id
);
