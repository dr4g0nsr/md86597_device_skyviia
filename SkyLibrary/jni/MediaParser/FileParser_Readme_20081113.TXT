FileParser Readme

(一). 可支援的格式:

	請參考 SK885X Supported Media Format-20080819.xls
			
	SK8850與SK8855主要的差異在於SK8855多支援VC-1, 其他大致相同.
	
	1. Video:
	
		判斷支援與否是只檢查檔案格式及video codec及video resolution, 並沒有檢查audio codec是否支援,
		所以如果是可支援的影片但播起來沒有聲音, 則可能是原本就沒聲音或者audio codec不支援的緣故.
			
		在可支援的檔案格式及video codec的條件下, 還有其他限制:
		
			(一)不支援的Video Profile:
			
				(a) H.264 Main profile: FileParser會顯示avc1(77)(H.264)
			
			(二)不支援的解析度限制(Width: W, Height: H):
			
				(a) SK8850(不支援VC-1):
				
					(a) H.264: W > 720 or H > 576
					(b) M-JPEG: W > 720 or H > 576
					(c) Other: W > 1440 or H > 960 (HW scalar的限制)
			
				(b) SK8855:
				
					(a) H.264: W > 720 or H > 576
					(b) VC-1: W x H > 352 x 288
					(c) Other: W > display width or H > display height
					    (因記憶體不足且速度不夠快, 所以不支援scale down)
				
				(c) FileParser:
		
					(a) 可選擇SK8850 or SK8855
					(b) 可選擇螢幕解析度
					(c) 判斷原則如(1)(2)所述
					
	2. Audio:
	
		目前只有發現兩種情況會被列為不支援.
		
		(1)AAC LTP
		(2)MP3的檔頭前有一些未知的資料或是多個ID3 tag

		若有其他情況, 請回報RD分析.
	
(二) 使用方法

	1. 可選擇SK8850 or SK8855
	2. 可選擇螢幕解析度: 720x480, 720x576, 800x600, 1920x1080
	3. 點選"File"以開啟要分析的檔案.
	4. 錯誤訊息:
	
		(a)"File format is not supported!"
		
			檔案格式不支援, 也可能是未知格式或檔頭不正確.
		
		(b)"Video codec is not supported!"
		
			檔案格式可支援, 但是video codec不支援.
			
		(c)"Video resolution is not supported!"

			檔案格式可支援, 但是因為解析度限制而不支援.

Update:

1. 2008/11/12: fix for GetMPGDuration
2. 2008/11/13: fix for new type ID3 tag
3. 2008/11/13: fix m4a duration


Edited by Raymond 2008/11/13
