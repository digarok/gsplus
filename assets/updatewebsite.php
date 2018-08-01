<?php

$d = file_get_contents($argv[1]);
$filter = (isset($argv[2])) ? $argv[2] : false;
$builds = [];
foreach(explode("\n", $d) as $line) {
    if (!stristr($line, 'gsplus')) { continue; }	// only gsplus files
    $data = explode(" ", $line);
    $data =  preg_split('/\s+/', $line);
    if (!$data || empty($data) || !isset($data[1])) { continue; }	// one more check for validity
    $b = explode("/", $data[3]);
    $time = $data[0] . ' ' . $data[1];
    $date = $data[0];
    $bytes = $data[2];
    $k = $b[0].$b[1].$b[2];  	// lookup key
    if (!$k) { continue; }	// something isn't right, skip
    $row = array('k' => $k, 'bytes' => $bytes, 'tag' => $b[0], 'version' => $b[1],
            'pipeline' => $b[2], 'platform' => $b[3], 'job' => $b[4],
            'time' => $time, 'date' => $date,
            'filename' => $b[5], 's3url' => 'https://s3.amazonaws.com/gsplus-artifacts/'.$data[3]
            );
    if (!$filter || ($filter && $b[0] == $filter)) {
        $builds[] = $row;
    }
}

$pipeline = array_column($builds, 'pipeline');
$platform = array_column($builds, 'platform');

array_multisort($pipeline, SORT_DESC, $platform, SORT_ASC, $builds);



$style = <<<EOF
.builddiv {
	width: 80%;
	-moz-border-radius: 1em 4em 1em 4em;
	border-radius: 1em 4em 1em 4em;
	padding: 2em;
	background-color: #fff;
	color: #000;
	margin: auto;
}
body { font-family: verdana, sans-serif;  font-size: 12px; color: #000; background-color: #18BC9C; font-style: normal; font-weight: normal; font-variant: normal; text-align: left; letter-spacing: 0px; line-height: 20px; }
EOF;

print("<html><head><title>GS+ builds page</title><style>$style</style></head><body>\n");
$lastk = null;
foreach ($builds as $l) {
    $k = $l['k'];
    if ($k != $lastk) {
	if ($lastk != null) {
		print("</div><br/><br/>");	// close last build if not first pass
	}
	print("<div class='builddiv'>\n");
        print("<h1>GSPLUS V".$l['version']." - Pipeline #".$l['pipeline']." @ ".$l['date']." <span style='font-size:0.r68em'>(".$l['tag'].")</span></H1><hr/>\n");
	$lastk = $k;
    }
    print("<h2>Platform: ".$l['platform']."</h2>\n");
    print("<ul><li>Build Job: ".$l['job']." &nbsp; Date: ".$l['time']." &nbsp; Size(Bytes): ".$l['bytes']."</li>\n");
    print("<li>Download: <a href='".$l['s3url']."'>".$l['filename']."</a></li></ul>\n");
}
print("</div></body></html>\n");

#2018-08-07 14:33:11     256833 auto/0.14rc/260/ubuntu-sdl/2481/gsplus-ubuntu-sdl.tar.bz2
