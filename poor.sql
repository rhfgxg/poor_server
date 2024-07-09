use poor;	-- 进入数据库

SHOW TABLES;	-- 查看可用表

DROP table log_client_connection;	-- 删除表

SELECT * from log_client_connection;	-- 查看表内数据

-- 用户信息表
CREATE TABLE `users` (
    `id` INT(11) NOT NULL AUTO_INCREMENT comment"用户id", -- 主键
	`username` VARCHAR(255) NOT NULL comment"用户名", -- 用户名
	`password_hash` VARCHAR(255) NOT NULL comment"md5密码", -- 密码md5加密
	`email` VARCHAR(255) comment"邮箱", -- 邮箱
	`phone_number` VARCHAR(20) comment"手机号", -- 手机号码
    `id_number` VARCHAR(20) comment"身份证号", -- 身份证号
    
	`registration_date` DATETIME NOT NULL comment"注册日期", -- 注册日期
	`last_login` DATETIME comment"最后一次登录时间", -- 最后一次登录时间
    
	`is_active` TINYINT(1) NOT NULL DEFAULT 1 comment"是否激活", -- 是否激活
	`role` ENUM('admin', 'user') NOT NULL DEFAULT 'user' comment"用户权限", -- 用户权限
    
    `avatar_url` VARCHAR(255) comment"头像链接",
    PRIMARY KEY (`id`),
	UNIQUE INDEX `username_UNIQUE` (`username` ASC) VISIBLE,	-- 创建一个索引，用来快速查询
	UNIQUE INDEX `email_UNIQUE` (`email` ASC) VISIBLE
);

-- 用户登录日志
CREATE TABLE `log_user_login`(
  `id` int(11) NOT NULL AUTO_INCREMENT comment"日志id",
  `user_id` int(11) NOT NULL comment"链接到用户id", -- 用户ID
  `login_time` datetime NOT NULL comment"登录时间", -- 登录时间
  `ip_address` varchar(45) NOT NULL comment"登录ip", -- IP地址
  `device_info` varchar(255) comment"登录设备信息", -- 设备信息
  `location` varchar(255) comment"登录位置", -- 地理位置
  PRIMARY KEY (`id`)
);

-- 客户端链接日志
CREATE TABLE `log_client_connection` (
  `id` int(11) NOT NULL AUTO_INCREMENT comment"日志id", -- 主键
  `client_id` varchar(255) NOT NULL comment"链接的用户id", -- 客户端ID或用户ID
  `connection_time` datetime NOT NULL comment"开始链接时间", -- 连接时间
  `disconnect_time` datetime comment"断开连接时间", -- 断开连接时间
  `ip_address` VARBINARY(16) NOT NULL comment"链接ip", -- IP地址
  `status` enum('connected','disconnected') NOT NULL comment"链接状态", -- 连接状态
  PRIMARY KEY (`id`)
);

-- 文件上传文件：记录所有上传中的文件，用于记录断点和缺失块，方便实现断点续传和文件完整
CREATE TABLE `upload_status_files` (
    `file_id` INT,			-- 文件ID，外键
    `user_id` INT NOT NULL,	-- 用户ID，外键
    `client_id` INT NOT NULL,	-- 客户端ID，防止多客户端同时上传文件
    `file_name` VARCHAR(255) NOT NULL,	-- 原文件名
    `file_format` VARCHAR(50) NOT NULL,	-- 文件格式：txt、jpg
    `actual_file_path` VARCHAR(255) NOT NULL,	-- 文件在服务端存储路径
    `file_size` BIGINT NOT NULL,				-- 文件总大小
    `disconnect_point` BIGINT DEFAULT 0,		-- 文件断联点，init为0，每次上传时更新。
    `last_upload_time` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,	-- 上次上传时间，用来标记断联时间
    `missing_parts` TEXT,		-- 缺失的文件块，使用字符串保存数字列表 表示未上传的文件块，使用数字1~n，空格进行间隔
    
    PRIMARY KEY (`file_id`),	-- 主键
    FOREIGN KEY (`file_id`) REFERENCES `completed_files`(`file_id`),	-- 外键文件表 文件id
	FOREIGN KEY (`user_id`) REFERENCES `users`(`id`)           	-- 外键关联到用户表 用户id
);

-- 用户上传文件表：记录用户上传完成的文件
CREATE TABLE `completed_files` (
    `file_id` INT PRIMARY KEY AUTO_INCREMENT,		-- 文件ID（服务器保存用的文件名）
    `user_id` INT NOT NULL,						-- 用户ID
    `file_name` VARCHAR(255) NOT NULL,			-- 原文件名
    `file_format` VARCHAR(50) NOT NULL,			-- 文件格式：txt、jpg
    `file_size` BIGINT NOT NULL,					-- 文件大小
    `upload_time` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,	-- 上传时间：以第一次上传时间为准
    `actual_file_path` VARCHAR(255) NOT NULL,		-- 文件在服务端保存路径（含文件名）
    `client_display_path` VARCHAR(255),			-- 客户端展示路径（含文件名）
    `file_status` ENUM('uploading', 'upload_failed', 'normal', 'deleted', 'blocked', 'corrupted', 'lost'),	-- 状态可以是 正常'normal', 删除'deleted', 被屏蔽'blocked', 损坏'corrupted', 丢失'missing'
	
    FOREIGN KEY (`user_id`) REFERENCES `users`(`id`)           -- 外键关联到用户表用户id
);
