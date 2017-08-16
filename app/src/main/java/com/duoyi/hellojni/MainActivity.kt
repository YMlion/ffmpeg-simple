package com.duoyi.hellojni

import android.os.Bundle
import android.os.Environment
import android.support.design.widget.Snackbar
import android.util.Log
import android.view.*
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.content_main.*

class MainActivity : BaseActivity() {

    var surfaceHolder: SurfaceHolder? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        setSupportActionBar(toolbar)

        fab.setOnClickListener { view ->
            Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                    .setAction("Action", null).show()
        }

        play_btn.setOnClickListener {
            playVideo(sample_text.text.toString())
        }

        // Example of a call to a native method
//        sample_text.text = stringFromJNI()
    }

    override fun onResume() {
        super.onResume()
        surfaceHolder = surface_view.holder
        surfaceHolder!!.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceChanged(holder: SurfaceHolder?, format: Int, width: Int, height: Int) {
            }

            override fun surfaceDestroyed(holder: SurfaceHolder?) {
            }

            override fun surfaceCreated(holder: SurfaceHolder?) {
//                playVideo("Video/videoplayback.mp4")
                var folderUrl: String = Environment.getExternalStorageDirectory().path
//                var inputUrl: String = folderUrl + "/DCIM/100MEDIA/VIDEO0005.mp4"
                var inputUrl: String = folderUrl + "/video10s.mp4"
                Log.d("TAG", inputUrl)
                var thread = Thread({
                    play(inputUrl, surfaceHolder!!.surface)
                })

                thread.start()
            }

        })
    }

    private fun playVideo(path : String) {
        var folderUrl: String = Environment.getExternalStorageDirectory().path
        var inputUrl: String = folderUrl + "/" + path
        Log.d("TAG", inputUrl)
        var thread = Thread({
            play(inputUrl, surfaceHolder!!.surface)
        })

        thread.start()
    }

    override fun setForegroundColor(decorView: ViewGroup) {
        /*val view = View(this)
        val lp = ViewGroup.LayoutParams(-1, -1)
        view.layoutParams = lp
        view.setBackgroundColor(Color.parseColor("#33C6C3C3"))
        decorView.addView(view)*/
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_settings -> true
            else -> super.onOptionsItemSelected(item)
        }
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
//    external fun stringFromJNI(): String
    external fun play(url: String, surface: Surface): Int

    companion object {

        // Used to load the 'native-lib' library on application startup.
        init {
            System.loadLibrary("native-lib")
            System.loadLibrary("avcodec")
            System.loadLibrary("avfilter")
            System.loadLibrary("avformat")
            System.loadLibrary("avutil")
            System.loadLibrary("swresample")
            System.loadLibrary("swscale")
        }
    }
}
